// SPDX-License-Identifier: MIT
#include "fakenetwork.h"
#include "services/poeauth.h"

#include <QEventLoop>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// made-up, and port 0 takes any free one
const PoeConfig config{.clientId = u"examplepou"_s, .contact = u"example.invalid"_s, .ports = {0}};

const QByteArray goodTokens = R"({"access_token":"fake-access","expires_in":36000,"token_type":"bearer",)"
                              R"("scope":"account:characters","refresh_token":"fake-refresh"})";

QUrlQuery queryOf(const QUrl &url)
{
    return QUrlQuery(url);
}

quint16 portOf(const QUrl &authorize)
{
    return QUrl(queryOf(authorize).queryItemValue(u"redirect_uri"_s, QUrl::FullyDecoded)).port();
}

// Plays the browser: goes to the redirect address with this query.
void browserGoesBack(const QUrl &authorize, const QString &query)
{
    auto *socket = new QTcpSocket;
    QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    QObject::connect(socket, &QTcpSocket::connected, socket, [socket, query] {
        socket->write("GET /callback?" + query.toUtf8() + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    });
    socket->connectToHost(QHostAddress::LocalHost, portOf(authorize));
}

// The page the browser got for that query.
QByteArray browserGets(const QUrl &authorize, const QString &query)
{
    QTcpSocket socket;
    QByteArray got;
    QEventLoop loop;
    QObject::connect(&socket, &QTcpSocket::connected, &socket,
                     [&] { socket.write("GET /callback?" + query.toUtf8() + " HTTP/1.1\r\n\r\n"); });
    QObject::connect(&socket, &QTcpSocket::readyRead, &socket, [&] { got += socket.readAll(); });
    QObject::connect(&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    socket.connectToHost(QHostAddress::LocalHost, portOf(authorize));
    loop.exec();
    return got + socket.readAll();
}

QUrlQuery sentBody(const FakeNetwork &network, int which = 0)
{
    return QUrlQuery(QString::fromUtf8(network.sent.at(which).body));
}

} // namespace

class TestPoeAuth : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void linksAnAccount()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.body = goodTokens};
        PoeAuth auth(network, config);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy linked(&auth, &PoeAuth::linked);
        QSignalSpy failed(&auth, &PoeAuth::failed);

        auth.start();
        QVERIFY(auth.isWaiting());
        QCOMPARE(browser.size(), 1);
        const QUrl authorize = browser.first().first().toUrl();
        QCOMPARE(authorize.host(), u"www.pathofexile.com"_s);
        const QString state = queryOf(authorize).queryItemValue(u"state"_s);

        browserGoesBack(authorize, u"code=fake-code&state="_s + state);
        QTRY_COMPARE(linked.size(), 1);
        QCOMPARE(failed.size(), 0);
        QVERIFY(!auth.isWaiting());

        const auto tokens = linked.first().first().value<PoeTokens>();
        QCOMPARE(tokens.access, u"fake-access"_s);
        QCOMPARE(tokens.refresh, u"fake-refresh"_s);
        QCOMPARE(tokens.expiresIn, 36000);

        // what it sent for the code: the verifier whose challenge it opened with
        QCOMPARE(network.sent.size(), 1);
        const auto &request = network.sent.first().request;
        QCOMPARE(request.url().toString(), u"https://www.pathofexile.com/oauth/token"_s);
        QCOMPARE(network.sent.first().operation, QNetworkAccessManager::PostOperation);
        QVERIFY(
            request.header(QNetworkRequest::UserAgentHeader).toByteArray().startsWith("OAuth examplepou/"));
        QCOMPARE(request.header(QNetworkRequest::ContentTypeHeader).toString(),
                 u"application/x-www-form-urlencoded"_s);
        const QUrlQuery body = sentBody(network);
        QCOMPARE(body.queryItemValue(u"grant_type"_s), u"authorization_code"_s);
        QCOMPARE(body.queryItemValue(u"code"_s), u"fake-code"_s);
        QCOMPARE(pkceChallenge(body.queryItemValue(u"code_verifier"_s)),
                 queryOf(authorize).queryItemValue(u"code_challenge"_s));
        QCOMPARE(body.queryItemValue(u"redirect_uri"_s, QUrl::FullyDecoded),
                 queryOf(authorize).queryItemValue(u"redirect_uri"_s, QUrl::FullyDecoded));
    }

    void listensOnTheRegisteredPort()
    {
        // two taken ports and a free one, in that order
        QTcpServer taken;
        QVERIFY(taken.listen(QHostAddress::LocalHost, 0));
        FakeNetwork network;
        PoeConfig mine = config;
        mine.ports = {taken.serverPort(), 0};
        PoeAuth auth(network, mine);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        auth.start();
        QCOMPARE(browser.size(), 1);
        QVERIFY(portOf(browser.first().first().toUrl()) != taken.serverPort());
    }

    void saysWhenNoPortIsFree()
    {
        QTcpServer taken;
        QVERIFY(taken.listen(QHostAddress::LocalHost, 0));
        FakeNetwork network;
        PoeConfig mine = config;
        mine.ports = {taken.serverPort()};
        PoeAuth auth(network, mine);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.start();
        QCOMPARE(browser.size(), 0);
        QCOMPARE(failed.size(), 1);
        QVERIFY(failed.first().first().toString().contains(u"ports"_s));
        QVERIFY(!auth.isWaiting());
    }

    void denied()
    {
        FakeNetwork network;
        PoeAuth auth(network, config);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.start();
        const QUrl authorize = browser.first().first().toUrl();
        browserGoesBack(authorize,
                        u"error=access_denied&state="_s + queryOf(authorize).queryItemValue(u"state"_s));
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(), u"You didn't allow it, so nothing was linked."_s);
        QVERIFY(!auth.isWaiting());
        QVERIFY(network.sent.isEmpty());
    }

    void ignoresAnAnswerFromAnotherSignIn()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.body = goodTokens};
        PoeAuth auth(network, config);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        QSignalSpy linked(&auth, &PoeAuth::linked);
        auth.start();
        const QUrl authorize = browser.first().first().toUrl();

        // another page sending the browser here, with a code or a denial
        const QByteArray page = browserGets(authorize, u"code=fake-code&state=not-ours"_s);
        QVERIFY(page.contains("<p>This answer wasn't for the sign in Path of Upgrading started. "
                              "You can close this tab.</p>"));
        QVERIFY(browserGets(authorize, u"error=access_denied&state=not-ours"_s)
                    .contains("wasn't for the sign in"));
        QVERIFY(browserGets(authorize, u"code=fake-code"_s).contains("wasn't for the sign in"));
        // it neither ends the sign in nor trades the code
        QVERIFY(auth.isWaiting());
        QCOMPARE(failed.size(), 0);
        QCOMPARE(linked.size(), 0);
        QVERIFY(network.sent.isEmpty());

        // and the real answer still works
        const QByteArray ours = browserGets(authorize, u"code=fake-code&state="_s +
                                                           queryOf(authorize).queryItemValue(u"state"_s));
        QVERIFY(ours.contains(
            "<p>Path of Upgrading got the answer. You can close this tab and go back to it.</p>"));
        QTRY_COMPARE(linked.size(), 1);
    }

    void unreadableAnswer()
    {
        FakeNetwork network;
        PoeAuth auth(network, config);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.start();
        const QUrl authorize = browser.first().first().toUrl();
        browserGoesBack(authorize, u"state="_s + queryOf(authorize).queryItemValue(u"state"_s));
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(network.sent.isEmpty());
    }

    void givesUpWaiting()
    {
        FakeNetwork network;
        PoeAuth auth(network, config);
        auth.setWaitLimit(std::chrono::milliseconds(40));
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.start();
        const quint16 port = portOf(browser.first().first().toUrl());
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(),
                 u"Nothing came back from the browser in time. Try linking again."_s);
        QVERIFY(!auth.isWaiting());
        // and stopped listening
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(!socket.waitForConnected(500));
    }

    void cancelSaysNothing()
    {
        FakeNetwork network;
        PoeAuth auth(network, config);
        auth.setWaitLimit(std::chrono::milliseconds(40));
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.start();
        const quint16 port = portOf(browser.first().first().toUrl());
        auth.cancel();
        QVERIFY(!auth.isWaiting());
        QTest::qWait(120);
        QCOMPARE(failed.size(), 0);
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(!socket.waitForConnected(500));
    }

    void startingAgainCallsTheFirstOff()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.body = goodTokens};
        PoeAuth auth(network, config);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy linked(&auth, &PoeAuth::linked);
        auth.start();
        auth.start();
        QCOMPARE(browser.size(), 2);
        const QUrl first = browser.at(0).first().toUrl();
        const QUrl second = browser.at(1).first().toUrl();
        QVERIFY(queryOf(first).queryItemValue(u"state"_s) != queryOf(second).queryItemValue(u"state"_s));
        // an answer to the first is no good any more, and doesn't end the second
        QSignalSpy failed(&auth, &PoeAuth::failed);
        browserGets(second, u"code=fake-code&state="_s + queryOf(first).queryItemValue(u"state"_s));
        QVERIFY(auth.isWaiting());
        QCOMPARE(failed.size(), 0);
        QCOMPARE(linked.size(), 0);
    }

    void ignoresStrayRequests()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.body = goodTokens};
        PoeAuth auth(network, config);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy linked(&auth, &PoeAuth::linked);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.start();
        const QUrl authorize = browser.first().first().toUrl();

        auto *socket = new QTcpSocket;
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QTcpSocket::connected, socket,
                         [socket] { socket->write("GET /favicon.ico HTTP/1.1\r\n\r\n"); });
        socket->connectToHost(QHostAddress::LocalHost, portOf(authorize));
        QTest::qWait(100);
        QVERIFY(auth.isWaiting());
        QCOMPARE(failed.size(), 0);

        browserGoesBack(authorize,
                        u"code=fake-code&state="_s + queryOf(authorize).queryItemValue(u"state"_s));
        QTRY_COMPARE(linked.size(), 1);
    }

    void aRefusedCodeAsksToLinkAgain()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 400, .body = R"({"error":"invalid_grant"})"};
        PoeAuth auth(network, config);
        QSignalSpy browser(&auth, &PoeAuth::browserWanted);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.start();
        const QUrl authorize = browser.first().first().toUrl();
        browserGoesBack(authorize, u"code=old&state="_s + queryOf(authorize).queryItemValue(u"state"_s));
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(), u"Your link ran out. Link your account again."_s);
        QCOMPARE(failed.first().at(1).toBool(), true);
    }

    void cantReachTheWebsite()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.error = QNetworkReply::HostNotFoundError};
        PoeAuth auth(network, config);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.refresh(u"fake-refresh"_s);
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(),
                 u"Couldn't reach the website. Check your connection and try again."_s);
        QCOMPARE(failed.first().at(1).toBool(), false);
    }

    void refreshes()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{
            .body = R"({"access_token":"newer","expires_in":36000,"refresh_token":"newer-refresh"})"};
        PoeAuth auth(network, config);
        QSignalSpy refreshed(&auth, &PoeAuth::refreshed);
        QSignalSpy linked(&auth, &PoeAuth::linked);
        auth.refresh(u"fake-refresh"_s);
        QTRY_COMPARE(refreshed.size(), 1);
        QCOMPARE(linked.size(), 0);
        QCOMPARE(refreshed.first().first().value<PoeTokens>().refresh, u"newer-refresh"_s);
        const QUrlQuery body = sentBody(network);
        QCOMPARE(body.queryItemValue(u"grant_type"_s), u"refresh_token"_s);
        QCOMPARE(body.queryItemValue(u"refresh_token"_s), u"fake-refresh"_s);
    }

    void refreshRefused()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 400, .body = R"({"error":"invalid_grant"})"};
        PoeAuth auth(network, config);
        QSignalSpy refreshed(&auth, &PoeAuth::refreshed);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.refresh(u"used-refresh"_s);
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(refreshed.size(), 0);
        QCOMPARE(failed.first().at(1).toBool(), true);
    }

    void rateLimitedAndOutages()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.status = 429, .body = "{}", .headers = {{"Retry-After", "30"}}}
                        << FakeAnswer{.status = 503};
        PoeAuth auth(network, config);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.refresh(u"fake-refresh"_s);
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.first().first().toString(),
                 u"The website says to wait 30 seconds before asking again."_s);
        QCOMPARE(failed.first().at(1).toBool(), false);
        auth.refresh(u"fake-refresh"_s);
        QTRY_COMPARE(failed.size(), 2);
        QVERIFY(failed.at(1).first().toString().contains(u"isn't answering"_s));
        // a refused answer isn't tried again, one request each
        QCOMPARE(network.sent.size(), 2);
    }

    void answerWithoutTokens()
    {
        FakeNetwork network;
        network.answers << FakeAnswer{.body = R"({"access_token":"a"})"};
        PoeAuth auth(network, config);
        QSignalSpy refreshed(&auth, &PoeAuth::refreshed);
        QSignalSpy failed(&auth, &PoeAuth::failed);
        auth.refresh(u"fake-refresh"_s);
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(refreshed.size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestPoeAuth)
#include "tst_poeauth.moc"
