// SPDX-License-Identifier: MIT
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "services/network.h"
#include "services/poeclient.h"
#include "services/tradeclient.h"
#include "services/tradesignin.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

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

// A sign in that takes a moment, as asking for a token would, and puts what it
// signs with in a header the site never reads.
class SlowSignIn : public TradeSignIn
{
public:
    QString error;     // when it can't sign
    int asked = 0;     // how many requests it was asked to sign
    int delivered = 0; // and how many it signed

    void sign(const QNetworkRequest &request, const QObject *context, Done done) override
    {
        ++asked;
        // the context goes, and so does the call, as it does for a real one
        QTimer::singleShot(10ms, context, [this, request, done] {
            ++delivered;
            QNetworkRequest signedIn = request;
            signedIn.setRawHeader("X-Test-Sign-In", "token-one");
            done(signedIn, error);
        });
    }
};

// The account's tokens, a moment after they're asked for, one new one after
// each that's rejected.
class FakeTokens : public PoeTokenSource
{
public:
    QString error; // when it can't give one
    int asked = 0;
    int rejected = 0;

    void withToken(const QObject *context, Handler handler) override
    {
        ++asked;
        const QString token = error.isEmpty() ? u"fake-token-%1"_s.arg(rejected + 1) : QString();
        QTimer::singleShot(0, context, [handler, token, this] { handler(token, error); });
    }
    void rejectToken() override { ++rejected; }
};

const QUrl apiBase(u"https://api.example.invalid/trade"_s);

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

    void rateLimited_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<std::chrono::seconds>("retryAfter");
        QTest::addColumn<QString>("why");

        const QByteArray tooMany = siteError(u"Rate limit exceeded"_s);
        QTest::newRow("429") << FakeAnswer{.status = 429, .body = tooMany, .headers = {{"Retry-After", "30"}}}
                             << 30s
                             << u"You've sent the trade site too many requests. Try again in 30 seconds."_s;
        QTest::newRow("in the message")
            << FakeAnswer{.status = 400, .body = tooMany, .headers = {{"Retry-After", "1"}}} << 1s
            << u"You've sent the trade site too many requests. Try again in 1 second."_s;
        // a minute when the site doesn't say how long
        QTest::newRow("unsaid")
            << FakeAnswer{.status = 429, .body = {}} << 60s
            << u"You've sent the trade site too many requests. Try again in 60 seconds."_s;
        QTest::newRow("two minutes")
            << FakeAnswer{.status = 429, .headers = {{"Retry-After", "120"}}} << 120s
            << u"You've sent the trade site too many requests. Try again in 2 minutes."_s;
        QTest::newRow("a long ban")
            << FakeAnswer{.status = 429, .headers = {{"Retry-After", "1741"}}} << 1741s
            << u"You've sent the trade site too many requests. Try again in 30 minutes."_s;
    }

    void rateLimited()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(std::chrono::seconds, retryAfter);
        QFETCH(QString, why);

        FakeNetwork network;
        network.answers << answer;
        TradeClient client(&network);
        TradeReply *reply = client.search(u"Standard"_s, {});
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::RateLimited);
        QCOMPARE(reply->retryAfter(), retryAfter);
        QCOMPARE(reply->errorString(), why);
    }

    void tooComplex()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 400, .body = siteError(u"Query is too complex."_s)};
        TradeClient client(&network);
        TradeReply *reply = client.search(u"Standard"_s, {});
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::TooComplex);
        QCOMPARE(reply->siteMessage(), u"Query is too complex."_s);
    }

    void rejected()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 400, .body = siteError(u"Invalid query"_s)};
        TradeClient client(&network);
        TradeReply *reply = client.search(u"Standard"_s, {});
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::Rejected);
        QCOMPARE(reply->errorString(), u"The trade site refused the search: Invalid query"_s);
        QCOMPARE(reply->siteMessage(), u"Invalid query"_s);
    }

    void unreachable()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.error = QNetworkReply::HostNotFoundError}
                        // a transfer timeout, which older versions of Qt report as canceled
                        << FakeAnswer{.error = QNetworkReply::OperationCanceledError}
                        << FakeAnswer{.error = QNetworkReply::TimeoutError};
        TradeClient client(&network);
        client.setRest(0ms);

        TradeReply *offline = client.data(u"stats"_s);
        QVERIFY(finishes(offline));
        QCOMPARE(offline->error(), TradeReply::Error::Network);
        QCOMPARE(offline->errorString(),
                 u"Couldn't reach the trade site: Host www.pathofexile.com not found"_s);
        QVERIFY(offline->siteMessage().isEmpty());

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
        client.setRest(0ms);

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
        QSignalSpy waiting(fifth, &TradeReply::waiting);
        // once the first has been answered
        QTRY_COMPARE(network.sent.size(), 4);
        QVERIFY(!waiting.isEmpty() || waiting.wait());
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
        // in any order, as each kind waits on its own
        QSet<QUrl> went;
        for (qsizetype i = 3; i < network.sent.size(); ++i)
            went << network.sent[i].request.url();
        QCOMPARE(went, (QSet<QUrl>{tradeSearchUrl(u"Runes of Aldur"_s), tradeFetchUrl({u"x2"_s}, u"Ab3dE9"_s),
                                   tradeDataUrl(u"leagues"_s)}));
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
        // only the first went, which is how it found out
        QCOMPARE(network.sent.size(), 1);
    }

    void waitsOutAHold()
    {
        FakeNetwork network;
        // just under half a minute
        network.answers << FakeAnswer{.status = 429, .headers = {{"Retry-After", "29"}}};
        TradeClient client(&network);
        QVERIFY(finishes(client.search(u"Standard"_s, {})));

        TradeReply *next = client.search(u"Standard"_s, {});
        QSignalSpy waiting(next, &TradeReply::waiting);
        QVERIFY(waiting.wait());
        const auto wait = waiting.first().first().value<std::chrono::milliseconds>();
        QCOMPARE_GT(wait, 28s);
        QCOMPARE_LE(wait, 29s);
        QCOMPARE(network.sent.size(), 1);
    }

    void failsRatherThanWaitLong_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<std::chrono::seconds>("hold");
        // just over half a minute
        QTest::newRow("said") << FakeAnswer{.status = 429, .headers = {{"Retry-After", "31"}}} << 31s;
        QTest::newRow("unsaid") << FakeAnswer{.status = 429} << 60s;
    }

    void failsRatherThanWaitLong()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(std::chrono::seconds, hold);
        FakeNetwork network;
        network.answers << answer;
        TradeClient client(&network);
        QVERIFY(finishes(client.search(u"Standard"_s, {})));

        // it fails at once, but still says so from the event loop
        TradeReply *next = client.search(u"Standard"_s, {});
        QSignalSpy finished(next, &TradeReply::finished);
        QVERIFY(finished.wait());
        QCOMPARE(next->error(), TradeReply::Error::RateLimited);
        QCOMPARE(next->retryAfter(), hold);
        QCOMPARE(network.sent.size(), 1);
    }

    void blockedSendsNothing()
    {
        FakeNetwork network;
        TradeClient client(&network);
        client.setBlocked(u"Trade searches are turned off."_s);
        QCOMPARE(client.blocked(), u"Trade searches are turned off."_s);

        const QList<TradeReply *> replies{client.search(u"Standard"_s, {}),
                                          client.fetch({u"x1"_s}, u"Ab3dE9"_s), client.data(u"stats"_s)};
        for (TradeReply *reply : replies) {
            // from the event loop, as the others are
            QVERIFY(!reply->isFinished() || reply->error() != TradeReply::Error::None);
            QVERIFY(finishes(reply));
            QCOMPARE(reply->error(), TradeReply::Error::Unavailable);
            QCOMPARE(reply->errorString(), u"Trade searches are turned off."_s);
        }
        QVERIFY(network.sent.isEmpty());

        // and goes again once it's let
        client.setBlocked({});
        QVERIFY(finishes(client.data(u"stats"_s)));
        QCOMPARE(network.sent.size(), 1);
    }

    void blockingCallsOffWhatWaits()
    {
        FakeNetwork network;
        TradeClient client(&network);
        TradeReply *fifth = nullptr;
        for (int i = 0; i < 5; ++i)
            fifth = client.search(u"Standard"_s, {});
        QCOMPARE(network.sent.size(), 1);

        client.setBlocked(u"Trade searches are turned off."_s);
        QVERIFY(finishes(fifth));
        QCOMPARE(fifth->error(), TradeReply::Error::Unavailable);
        // and the ones held for the answer don't go when it comes
        QTest::qWait(30);
        QCOMPARE(network.sent.size(), 1);
    }

    void oneAtATimeUntilTheSiteAnswers()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.never = true} << FakeAnswer{.never = true};
        TradeClient client(&network);
        const QList<TradeReply *> replies{client.search(u"Standard"_s, {}), client.search(u"Standard"_s, {}),
                                          client.search(u"Standard"_s, {})};
        QTest::qWait(50);
        QCOMPARE(network.sent.size(), 1);

        // calling off the one that's out lets the next go, still alone
        delete replies[0];
        QCOMPARE(network.sent.size(), 2);
        QTest::qWait(50);
        QCOMPARE(network.sent.size(), 2);
    }

    void manyOnceTheSiteHasAnswered()
    {
        FakeNetwork network;
        TradeClient client(&network);
        QList<TradeReply *> replies;
        for (int i = 0; i < 3; ++i)
            replies << client.search(u"Standard"_s, {});
        QCOMPARE(network.sent.size(), 1);
        for (TradeReply *reply : replies)
            QVERIFY(finishes(reply));
        QCOMPARE(network.sent.size(), 3);
        // and each kind is waited for apart
        QCOMPARE(network.sent.size(), 3);
        TradeReply *fetch = client.fetch({u"x1"_s}, u"Ab3dE9"_s);
        TradeReply *data = client.data(u"stats"_s);
        QCOMPARE(network.sent.size(), 5);
        QVERIFY(finishes(fetch) && finishes(data));
    }

    void stillOneAtATimeAfterNoAnswer()
    {
        FakeNetwork network;
        // as when it couldn't be reached, which says nothing of its limits
        network.answers << FakeAnswer{.error = QNetworkReply::HostNotFoundError} << FakeAnswer{.never = true};
        TradeClient client(&network);
        client.setRest(0ms);
        const TradeReply *first = client.search(u"Standard"_s, {});
        client.search(u"Standard"_s, {});
        client.search(u"Standard"_s, {});
        QTRY_COMPARE(network.sent.size(), 2);
        QTest::qWait(50);
        QCOMPARE(network.sent.size(), 2);
        QVERIFY(first->isFinished());
    }

    void restsAfterAFailure()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.error = QNetworkReply::HostNotFoundError} << FakeAnswer{.status = 500};
        TradeClient client(&network);
        client.setRest(300ms);
        QVERIFY(finishes(client.search(u"Standard"_s, {})));

        // it waits out the rest, which is said as any other wait
        QElapsedTimer clock;
        clock.start();
        TradeReply *next = client.data(u"stats"_s);
        QSignalSpy waiting(next, &TradeReply::waiting);
        QVERIFY(waiting.wait());
        const auto wait = waiting.first().first().value<std::chrono::milliseconds>();
        QCOMPARE_GT(wait, 200ms);
        QCOMPARE_LE(wait, 300ms);
        QCOMPARE(network.sent.size(), 1);
        QVERIFY(finishes(next));
        QCOMPARE_GE(clock.elapsed(), 280);
        QCOMPARE(network.sent.size(), 2);
        QCOMPARE(next->error(), TradeReply::Error::BadAnswer);

        // twice as long after the next, and the same kind of wait for any kind
        clock.restart();
        TradeReply *third = client.fetch({u"x1"_s}, u"Ab3dE9"_s);
        QVERIFY(finishes(third));
        QCOMPARE_GE(clock.elapsed(), 580);
        QCOMPARE(network.sent.size(), 3);
        QCOMPARE(third->error(), TradeReply::Error::None);

        // and what it answers puts that right
        QElapsedTimer quick;
        quick.start();
        QVERIFY(finishes(client.search(u"Standard"_s, {})));
        QCOMPARE_LT(quick.elapsed(), 200);
    }

    void failsRatherThanRestLong()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.error = QNetworkReply::HostNotFoundError};
        TradeClient client(&network);
        client.setRest(40s);
        QVERIFY(finishes(client.search(u"Standard"_s, {})));

        TradeReply *next = client.search(u"Standard"_s, {});
        QVERIFY(finishes(next));
        QCOMPARE(next->error(), TradeReply::Error::Network);
        QCOMPARE(next->retryAfter(), 40s);
        QCOMPARE(
            next->errorString(),
            u"The trade site isn't answering, so the app is giving it a rest. Try again in 40 seconds."_s);
        QCOMPARE(network.sent.size(), 1);
    }

    void aSiteThatSaysNoIsNotResting()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 400, .body = siteError(u"Unknown item base"_s)};
        TradeClient client(&network);
        client.setRest(40s);
        QVERIFY(finishes(client.search(u"Standard"_s, {})));
        QVERIFY(finishes(client.search(u"Standard"_s, {})));
        QCOMPARE(network.sent.size(), 2);
    }

    void sessionIds()
    {
        QVERIFY(TradeClient::isSessionId(u"0123456789abcdef0123456789abcdef"));
        QVERIFY(TradeClient::isSessionId(u"0123456789ABCDEF"));
        QVERIFY(!TradeClient::isSessionId(u""));
        // more than the cookie's value, which would go out as it's written
        QVERIFY(!TradeClient::isSessionId(u"POESESSID=0123456789abcdef"));
        QVERIFY(!TradeClient::isSessionId(u"0123456789abcdef; other=1"));
        QVERIFY(!TradeClient::isSessionId(u"0123456789abcdef\r\nX-Other: 1"));
        QVERIFY(!TradeClient::isSessionId(u"0123456789abcd\u00e9f"));
    }

    void signedIn()
    {
        FakeNetwork network;
        TradeClient client(&network);
        // as pasted from the browser
        client.setSessionId(u" 0123456789abcdef0123456789abcdef\n"_s);
        QVERIFY(finishes(client.search(u"Standard"_s, {})));
        QVERIFY(finishes(client.fetch({u"x1"_s}, u"Ab3dE9"_s)));
        QVERIFY(finishes(client.data(u"leagues"_s)));
        for (const FakeNetwork::Sent &sent : std::as_const(network.sent)) {
            QCOMPARE(sent.request.rawHeader("Cookie"), "POESESSID=0123456789abcdef0123456789abcdef");
            // which the network's cookie jar would send its own in place of
            QCOMPARE(sent.request.attribute(QNetworkRequest::CookieLoadControlAttribute).toInt(),
                     int(QNetworkRequest::Manual));
            // nor keep the one the site sends back after you sign out
            QCOMPARE(sent.request.attribute(QNetworkRequest::CookieSaveControlAttribute).toInt(),
                     int(QNetworkRequest::Manual));
            // and a redirect elsewhere would send it there as well
            QCOMPARE(sent.request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                     int(QNetworkRequest::SameOriginRedirectPolicy));
        }

        client.setSessionId(u"0123456789abcdef; other=1"_s);
        QVERIFY(finishes(client.data(u"leagues"_s)));
        client.setSessionId({});
        QVERIFY(finishes(client.data(u"leagues"_s)));
        for (const FakeNetwork::Sent &sent : network.sent.mid(3)) {
            QVERIFY(!sent.request.hasRawHeader("Cookie"));
            QVERIFY(!sent.request.attribute(QNetworkRequest::CookieLoadControlAttribute).isValid());
            QVERIFY(!sent.request.attribute(QNetworkRequest::CookieSaveControlAttribute).isValid());
            QVERIFY(!sent.request.attribute(QNetworkRequest::RedirectPolicyAttribute).isValid());
        }
    }

    void theAccountsTokenGoesToItsOwnAddressAndNowhereElse()
    {
        FakeNetwork network;
        FakeTokens tokens;
        TradeClient client(&network);
        client.setSignIn(std::make_unique<BearerSignIn>(&tokens, apiBase));
        QVERIFY(finishes(client.search(u"Standard"_s, {})));
        QVERIFY(finishes(client.fetch({u"x1"_s}, u"Ab3dE9"_s)));
        QVERIFY(finishes(client.data(u"leagues"_s)));
        QCOMPARE(tokens.asked, 3);
        QCOMPARE(network.sent.size(), 3);
        QCOMPARE(network.sent.at(0).request.url(),
                 QUrl(u"https://api.example.invalid/trade/search/poe2/Standard"_s));
        QCOMPARE(network.sent.at(2).request.url(), QUrl(u"https://api.example.invalid/trade/data/leagues"_s));
        for (const FakeNetwork::Sent &sent : std::as_const(network.sent)) {
            QCOMPARE(sent.request.url().host(), u"api.example.invalid"_s);
            QCOMPARE(sent.request.rawHeader("Authorization"), "Bearer fake-token-1");
            // no cookie, from us or the jar
            QVERIFY(!sent.request.hasRawHeader("Cookie"));
            QCOMPARE(sent.request.attribute(QNetworkRequest::CookieLoadControlAttribute).toInt(),
                     int(QNetworkRequest::Manual));
            // and a redirect to another host doesn't get the token
            QCOMPARE(sent.request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                     int(QNetworkRequest::SameOriginRedirectPolicy));
        }
    }

    void theBearerSignInTurnsDownWhatIsntTheSitesOwn()
    {
        FakeTokens tokens;
        BearerSignIn signIn(&tokens, apiBase);
        QObject context;
        for (const QString &address : {u"https://elsewhere.invalid/api/trade2/data/stats"_s,
                                       u"http://www.pathofexile.com/api/trade2/data/stats"_s,
                                       u"https://www.pathofexile.com/oauth/token"_s}) {
            int calls = 0;
            QString why;
            signIn.sign(QNetworkRequest(QUrl(address)), &context,
                        [&](const QNetworkRequest &, const QString &e) {
                            ++calls;
                            why = e;
                        });
            // at once, and with no token asked for
            QCOMPARE(calls, 1);
            QVERIFY(!why.isEmpty());
        }
        QCOMPARE(tokens.asked, 0);
    }

    void withoutATokenNothingIsSent()
    {
        FakeNetwork network;
        FakeTokens tokens;
        tokens.error = u"Your Path of Exile link has run out."_s;
        TradeClient client(&network);
        client.setSignIn(std::make_unique<BearerSignIn>(&tokens, apiBase));
        TradeReply *reply = client.search(u"Standard"_s, {});
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::Rejected);
        QCOMPARE(reply->errorString(), u"Your Path of Exile link has run out."_s);
        QCOMPARE(network.sent.size(), 0);
        QCOMPARE(tokens.rejected, 0);
    }

    void aRefusedTokenIsReplacedOnceAndTheRequestTriedAgain()
    {
        FakeNetwork network;
        const QJsonObject found{{u"id"_s, u"Ab3dE9"_s}, {u"total"_s, 0}, {u"result"_s, QJsonArray()}};
        network.answers << FakeAnswer{.status = 401, .body = "{}"} << FakeAnswer{.body = json(found)};
        FakeTokens tokens;
        TradeClient client(&network);
        client.setSignIn(std::make_unique<BearerSignIn>(&tokens, apiBase));
        const QJsonObject body{{u"query"_s, QJsonObject{{u"type"_s, u"Sapphire Ring"_s}}}};
        TradeReply *reply = client.search(u"Standard"_s, body);
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::None);
        QCOMPARE(reply->answer(), found);
        QCOMPARE(tokens.rejected, 1);
        QCOMPARE(tokens.asked, 2);
        QCOMPARE(network.sent.size(), 2);
        QCOMPARE(network.sent.at(0).request.rawHeader("Authorization"), "Bearer fake-token-1");
        QCOMPARE(network.sent.at(1).request.rawHeader("Authorization"), "Bearer fake-token-2");
        // the same request, body and all
        QCOMPARE(network.sent.at(1).request.url(), network.sent.at(0).request.url());
        QCOMPARE(QJsonDocument::fromJson(network.sent.at(1).body).object(), body);
    }

    void aSecondRefusalIsTheAnswerAndNothingIsSentAgain()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 403, .body = "{}"} << FakeAnswer{.status = 403, .body = "{}"};
        FakeTokens tokens;
        TradeClient client(&network);
        // as a site that says nothing useful is given a rest
        client.setRest(1ms);
        client.setSignIn(std::make_unique<BearerSignIn>(&tokens, apiBase));
        TradeReply *reply = client.data(u"stats"_s);
        QVERIFY(finishes(reply));
        QVERIFY(reply->error() != TradeReply::Error::None);
        QCOMPARE(network.sent.size(), 2);
        QCOMPARE(tokens.rejected, 1);
        QCOMPARE(tokens.asked, 2);
        // and the next request is a new one, with its own once
        QVERIFY(finishes(client.data(u"leagues"_s)));
        QCOMPARE(network.sent.size(), 3);
    }

    void onlyARefusalIsTriedAgain()
    {
        FakeNetwork network;
        FakeTokens tokens;
        TradeClient client(&network);
        client.setRest(1ms);
        client.setSignIn(std::make_unique<BearerSignIn>(&tokens, apiBase));
        network.answers << FakeAnswer{.status = 400, .body = siteError(u"Unknown item base"_s)}
                        << FakeAnswer{.status = 500, .body = "oops"};
        QVERIFY(finishes(client.data(u"stats"_s)));
        QVERIFY(finishes(client.data(u"leagues"_s)));
        QCOMPARE(network.sent.size(), 2);
        QCOMPARE(tokens.rejected, 0);
    }

    void aSessionIdIsNeverTriedAgain()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 403, .body = "{}"};
        TradeClient client(&network);
        client.setSessionId(u"0123456789abcdef"_s);
        QVERIFY(finishes(client.data(u"stats"_s)));
        QCOMPARE(network.sent.size(), 1);
    }

    void aHandedSignInSignsEveryKindOfRequest()
    {
        FakeNetwork network;
        TradeClient client(&network);
        auto signIn = std::make_unique<SlowSignIn>();
        SlowSignIn *slow = signIn.get();
        client.setSignIn(std::move(signIn));
        QVERIFY(finishes(client.search(u"Standard"_s, {})));
        QVERIFY(finishes(client.fetch({u"x1"_s}, u"Ab3dE9"_s)));
        QVERIFY(finishes(client.data(u"leagues"_s)));
        QCOMPARE(slow->delivered, 3);
        QCOMPARE(network.sent.size(), 3);
        for (const FakeNetwork::Sent &sent : std::as_const(network.sent))
            QCOMPARE(sent.request.rawHeader("X-Test-Sign-In"), "token-one");
    }

    void aSignInReplacesTheSessionIdAndTheOtherWayRound()
    {
        FakeNetwork network;
        TradeClient client(&network);
        client.setSessionId(u"0123456789abcdef"_s);
        client.setSignIn(std::make_unique<SlowSignIn>());
        QVERIFY(finishes(client.data(u"leagues"_s)));
        QVERIFY(!network.sent.last().request.hasRawHeader("Cookie"));
        QVERIFY(network.sent.last().request.hasRawHeader("X-Test-Sign-In"));

        client.setSessionId(u"0123456789abcdef"_s);
        QVERIFY(finishes(client.data(u"leagues"_s)));
        QVERIFY(network.sent.last().request.hasRawHeader("Cookie"));
        QVERIFY(!network.sent.last().request.hasRawHeader("X-Test-Sign-In"));

        client.setSignIn(nullptr);
        QVERIFY(finishes(client.data(u"leagues"_s)));
        QVERIFY(!network.sent.last().request.hasRawHeader("Cookie"));
        QVERIFY(!network.sent.last().request.hasRawHeader("X-Test-Sign-In"));
    }

    void aSignInThatCantSignSendsNothing()
    {
        FakeNetwork network;
        TradeClient client(&network);
        auto signIn = std::make_unique<SlowSignIn>();
        signIn->error = u"The account isn't linked any more."_s;
        client.setSignIn(std::move(signIn));
        TradeReply *reply = client.search(u"Standard"_s, {});
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::Rejected);
        QCOMPARE(reply->errorString(), u"The account isn't linked any more."_s);
        QCOMPARE(network.sent.size(), 0);

        // and the next one is asked, not held up behind it
        QVERIFY(finishes(client.search(u"Standard"_s, {})));
    }

    void aRequestCalledOffWhileBeingSignedIsNeverSentAndFreesItsPlace()
    {
        FakeNetwork network;
        TradeClient client(&network);
        auto signIn = std::make_unique<SlowSignIn>();
        SlowSignIn *slow = signIn.get();
        client.setSignIn(std::move(signIn));
        TradeReply *first = client.data(u"stats"_s);
        // as the site hasn't answered yet, this one waits for the first
        TradeReply *second = client.data(u"leagues"_s);
        QCOMPARE(slow->asked, 1);
        delete first;
        QVERIFY(finishes(second));
        QCOMPARE(slow->delivered, 1);
        QCOMPARE(network.sent.size(), 1);
        QCOMPARE(network.sent.first().request.url(), tradeDataUrl(u"leagues"_s));
    }
};

QTEST_GUILESS_MAIN(TestTradeClient)
#include "tst_tradeclient.moc"
