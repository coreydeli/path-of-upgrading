// SPDX-License-Identifier: MIT
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "services/network.h"
#include "services/tradeclient.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

namespace {

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// What the site answers when it won't run a request.
QByteArray siteError(const QString &message)
{
    return json({{u"error"_s, QJsonObject{{u"code"_s, 2}, {u"message"_s, message}}}});
}

// Waits for the reply to finish, if it hasn't.
bool finishes(TradeReply *reply)
{
    QSignalSpy finished(reply, &TradeReply::finished);
    return reply->isFinished() || finished.wait();
}

} // namespace

class TestTradeClient : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void searches()
    {
        const QJsonObject found{
            {u"id"_s, u"Ab3dE9"_s},
            {u"total"_s, 2},
            {u"result"_s, QJsonArray{u"x1"_s, u"x2"_s}},
        };
        FakeNetwork network;
        network.answers << FakeAnswer{.body = json(found)};
        TradeClient client(&network);

        const QJsonObject body{{u"query"_s, QJsonObject{{u"type"_s, u"Sapphire Ring"_s}}}};
        TradeReply *reply = client.search(u"Runes of Aldur"_s, body);
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::None);
        QCOMPARE(reply->answer(), found);

        QCOMPARE(network.sent.size(), 1);
        const FakeNetwork::Sent &sent = network.sent.first();
        QCOMPARE(sent.operation, QNetworkAccessManager::PostOperation);
        QCOMPARE(sent.request.url(), tradeSearchUrl(u"Runes of Aldur"_s));
        QCOMPARE(sent.request.header(QNetworkRequest::ContentTypeHeader).toByteArray(), "application/json");
        QCOMPARE(sent.request.header(QNetworkRequest::UserAgentHeader).toByteArray(), userAgent());
        QCOMPARE(QJsonDocument::fromJson(sent.body).object(), body);

        // the network's reply goes once it has been read
        QTRY_VERIFY(sent.reply.isNull());
    }

    void fetchesAndData()
    {
        FakeNetwork network;
        TradeClient client(&network);
        QVERIFY(finishes(client.fetch({u"x1"_s, u"x2"_s}, u"Ab3dE9"_s)));
        QVERIFY(finishes(client.data(u"leagues"_s)));

        QCOMPARE(network.sent.size(), 2);
        QCOMPARE(network.sent[0].operation, QNetworkAccessManager::GetOperation);
        QCOMPARE(network.sent[0].request.url(), tradeFetchUrl({u"x1"_s, u"x2"_s}, u"Ab3dE9"_s));
        QCOMPARE(network.sent[1].operation, QNetworkAccessManager::GetOperation);
        QCOMPARE(network.sent[1].request.url(), tradeDataUrl(u"leagues"_s));
    }

    void rateLimited()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 429,
                                      .body = siteError(u"Rate limit exceeded"_s),
                                      .headers = {{"Retry-After", "30"}}}
                        << FakeAnswer{.status = 400,
                                      .body = siteError(u"Rate limit exceeded"_s),
                                      .headers = {{"Retry-After", "1"}}}
                        << FakeAnswer{.status = 429, .body = {}};
        TradeClient client(&network);

        TradeReply *reply = client.search(u"Standard"_s, {});
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::RateLimited);
        QCOMPARE(reply->retryAfter(), 30s);
        QCOMPARE(reply->errorString(),
                 u"The trade site has had too many requests from you. Try again in 30 seconds."_s);

        // said only in the message
        TradeReply *message = client.search(u"Standard"_s, {});
        QVERIFY(finishes(message));
        QCOMPARE(message->error(), TradeReply::Error::RateLimited);
        QCOMPARE(message->errorString(),
                 u"The trade site has had too many requests from you. Try again in 1 second."_s);

        // a minute when the site doesn't say how long
        TradeReply *bare = client.search(u"Standard"_s, {});
        QVERIFY(finishes(bare));
        QCOMPARE(bare->error(), TradeReply::Error::RateLimited);
        QCOMPARE(bare->retryAfter(), 60s);
    }

    void tooComplex()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 400, .body = siteError(u"Query is too complex."_s)};
        TradeClient client(&network);
        TradeReply *reply = client.search(u"Standard"_s, {});
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::TooComplex);
    }

    void rejected()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 400, .body = siteError(u"Invalid query"_s)};
        TradeClient client(&network);
        TradeReply *reply = client.search(u"Standard"_s, {});
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::Rejected);
        QCOMPARE(reply->errorString(), u"The trade site says: Invalid query"_s);
    }

    void unreachable()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.error = QNetworkReply::HostNotFoundError}
                        // a transfer timeout, which older versions of Qt report as canceled
                        << FakeAnswer{.error = QNetworkReply::OperationCanceledError}
                        << FakeAnswer{.error = QNetworkReply::TimeoutError};
        TradeClient client(&network);

        TradeReply *offline = client.data(u"stats"_s);
        QVERIFY(finishes(offline));
        QCOMPARE(offline->error(), TradeReply::Error::Network);
        QCOMPARE(offline->errorString(),
                 u"Couldn't reach the trade site: Host www.pathofexile.com not found"_s);

        for (int i = 0; i < 2; ++i) {
            TradeReply *slow = client.data(u"stats"_s);
            QVERIFY(finishes(slow));
            QCOMPARE(slow->error(), TradeReply::Error::Network);
            QCOMPARE(slow->errorString(), u"The trade site didn't answer in time."_s);
        }
    }

    void badAnswers()
    {
        FakeNetwork network;
        // a page in front of the site that won't let the app through
        network.answers << FakeAnswer{.status = 403, .body = "<!DOCTYPE html><title>Just a moment...</title>"}
                        << FakeAnswer{.body = "[]"} << FakeAnswer{.status = 500};
        TradeClient client(&network);

        TradeReply *blocked = client.search(u"Standard"_s, {});
        QVERIFY(finishes(blocked));
        QCOMPARE(blocked->error(), TradeReply::Error::BadAnswer);
        QCOMPARE(blocked->errorString(), u"The trade site's answer couldn't be read (HTTP 403)."_s);

        TradeReply *list = client.data(u"stats"_s);
        QVERIFY(finishes(list));
        QCOMPARE(list->error(), TradeReply::Error::BadAnswer);

        // JSON, but not an answer
        TradeReply *broken = client.data(u"stats"_s);
        QVERIFY(finishes(broken));
        QCOMPARE(broken->error(), TradeReply::Error::BadAnswer);
    }

    void callingOff()
    {
        FakeNetwork network;
        TradeClient client(&network);
        TradeReply *reply = client.search(u"Standard"_s, {});
        QSignalSpy finished(reply, &TradeReply::finished);
        delete reply;
        QCOMPARE(network.sent.size(), 1);
        QVERIFY(network.sent.first().reply.isNull());
        QTest::qWait(10);
        QCOMPARE(finished.count(), 0);
    }

    void deletingWhenFinished()
    {
        FakeNetwork network;
        TradeClient client(&network);
        TradeReply *reply = client.data(u"stats"_s);
        QVERIFY(!reply->isFinished());

        bool finished = false;
        connect(reply, &TradeReply::finished, this, [&finished, reply] {
            finished = reply->isFinished();
            delete reply;
        });
        QTRY_VERIFY(finished);
        QVERIFY(network.sent.first().reply.isNull());
    }

    void spacesRequestsOut()
    {
        FakeNetwork network;
        TradeClient client(&network);
        // four in ten seconds, under the safe guesses with one to spare
        TradeReply *fifth = nullptr;
        for (int i = 0; i < 5; ++i)
            fifth = client.search(u"Standard"_s, {});
        QCOMPARE(network.sent.size(), 4);

        QSignalSpy waiting(fifth, &TradeReply::waiting);
        QVERIFY(waiting.wait());
        const auto wait = waiting.first().first().value<std::chrono::milliseconds>();
        QCOMPARE_GT(wait, 9s);
        QCOMPARE_LE(wait, 10250ms);
        QVERIFY(!fifth->isFinished());

        // fetches are limited apart
        QVERIFY(finishes(client.fetch({u"x1"_s}, u"Ab3dE9"_s)));
        QCOMPARE(network.sent.size(), 5);
    }

    void heldRequestsGoInTurn()
    {
        FakeNetwork network;
        // one a second, of each kind
        const FakeAnswer oneASecond{.headers = {{"X-Rate-Limit-Ip", "1:1:0"}}};
        network.answers << oneASecond << oneASecond << oneASecond;
        TradeClient client(&network);
        QElapsedTimer clock;
        clock.start();
        QVERIFY(finishes(client.search(u"Standard"_s, {})));
        QVERIFY(finishes(client.fetch({u"x1"_s}, u"Ab3dE9"_s)));
        QVERIFY(finishes(client.data(u"stats"_s)));

        // part of the way to their turn
        QTest::qWait(500);
        TradeReply *calledOff = client.search(u"Hardcore"_s, {});
        const QList<TradeReply *> held{
            client.search(u"Runes of Aldur"_s, {}),
            client.fetch({u"x2"_s}, u"Ab3dE9"_s),
            client.data(u"leagues"_s),
        };
        QCOMPARE(network.sent.size(), 3);
        delete calledOff;
        for (TradeReply *reply : held)
            QVERIFY(finishes(reply));
        QCOMPARE_GE(clock.elapsed(), 1250);
        QCOMPARE(network.sent.size(), 6);
        QCOMPARE(network.sent[3].request.url(), tradeSearchUrl(u"Runes of Aldur"_s));
    }

    void countsRequestsFromElsewhere_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        // three a minute, of which the site has counted two
        QTest::newRow("address") << FakeAnswer{
            .headers = {{"X-Rate-Limit-Ip", "3:60:60"}, {"X-Rate-Limit-Ip-State", "2:60:0"}}};
        QTest::newRow("account") << FakeAnswer{
            .headers = {{"X-Rate-Limit-Account", "3:60:60"}, {"X-Rate-Limit-Account-State", "2:60:0"}}};
    }

    void countsRequestsFromElsewhere()
    {
        QFETCH(FakeAnswer, answer);
        FakeNetwork network;
        network.answers << answer;
        TradeClient client(&network);
        QVERIFY(finishes(client.search(u"Standard"_s, {})));

        // the one to spare is gone, and waiting for it would take too long
        TradeReply *next = client.search(u"Standard"_s, {});
        QVERIFY(finishes(next));
        QCOMPARE(next->error(), TradeReply::Error::RateLimited);
        QCOMPARE(network.sent.size(), 1);
    }

    void bannedWhileWaiting()
    {
        FakeNetwork network;
        // a minute's ban, from requests made elsewhere
        network.answers << FakeAnswer{
            .headers = {{"X-Rate-Limit-Ip", "5:10:60"}, {"X-Rate-Limit-Ip-State", "6:10:60"}}};
        TradeClient client(&network);
        TradeReply *fifth = nullptr;
        for (int i = 0; i < 5; ++i)
            fifth = client.search(u"Standard"_s, {});
        QVERIFY(finishes(fifth));
        QCOMPARE(fifth->error(), TradeReply::Error::RateLimited);
        QCOMPARE(network.sent.size(), 4);
    }
};

QTEST_GUILESS_MAIN(TestTradeClient)
#include "tst_tradeclient.moc"
