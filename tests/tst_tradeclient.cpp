// SPDX-License-Identifier: MIT
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "services/network.h"
#include "services/tradeclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool finishes(TradeReply *reply)
{
    QSignalSpy finished(reply, &TradeReply::finished);
    return finished.wait();
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
};

QTEST_GUILESS_MAIN(TestTradeClient)
#include "tst_tradeclient.moc"
