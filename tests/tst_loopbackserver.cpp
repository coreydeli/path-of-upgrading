// SPDX-License-Identifier: MIT
#include "services/loopbackserver.h"

#include <QEventLoop>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// Sends what a browser would and gives back what the server said, with the
// event loop running as the server is in this thread.
QByteArray ask(quint16 port, const QByteArray &request)
{
    QTcpSocket socket;
    QByteArray got;
    QEventLoop loop;
    QObject::connect(&socket, &QTcpSocket::connected, &socket, [&] { socket.write(request); });
    QObject::connect(&socket, &QTcpSocket::readyRead, &socket, [&] { got += socket.readAll(); });
    QObject::connect(&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    socket.connectToHost(QHostAddress::LocalHost, port);
    loop.exec();
    return got + socket.readAll();
}

} // namespace

class TestLoopbackServer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void takesTheCallback()
    {
        LoopbackServer server;
        QSignalSpy spy(&server, &LoopbackServer::requested);
        const quint16 port = server.listen({0});
        QVERIFY(port != 0);

        const QByteArray answer =
            ask(port, "GET /callback?code=abc&state=xyz HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
        QVERIFY(answer.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(answer.contains("<p>Path of Upgrading got the answer. You can close this tab.</p>"));
        QCOMPARE(spy.size(), 1);
        const QUrl url = spy.first().first().toUrl();
        QCOMPARE(url.path(), u"/callback"_s);
        QCOMPARE(url.query(), u"code=abc&state=xyz"_s);
    }

    void theCallerChoosesThePage()
    {
        LoopbackServer server;
        QUrl asked;
        server.setPage([&asked](const QUrl &url) {
            asked = url;
            return url.query().contains(u"bad"_s) ? u"That wasn't right."_s : u"That was right."_s;
        });
        QSignalSpy spy(&server, &LoopbackServer::requested);
        const quint16 port = server.listen({0});

        QVERIFY(ask(port, "GET /callback?bad=1 HTTP/1.1\r\n\r\n").contains("<p>That wasn't right.</p>"));
        QCOMPARE(asked.query(), u"bad=1"_s);
        QVERIFY(ask(port, "GET /callback?good=1 HTTP/1.1\r\n\r\n").contains("<p>That was right.</p>"));
        QCOMPARE(spy.size(), 2);
        // it isn't asked about what it doesn't take
        asked.clear();
        ask(port, "GET /favicon.ico HTTP/1.1\r\n\r\n");
        QVERIFY(asked.isEmpty());
    }

    void refusesALineThatNeverEnds()
    {
        LoopbackServer server;
        QSignalSpy spy(&server, &LoopbackServer::requested);
        const quint16 port = server.listen({0});
        QVERIFY(ask(port, "GET /callback?" + QByteArray(20000, 'a')).startsWith("HTTP/1.1 431"));
        QCOMPARE(spy.size(), 0);
    }

    void closingWithAConnectionOpenIsFine()
    {
        QTcpSocket socket;
        {
            LoopbackServer server;
            const quint16 port = server.listen({0});
            socket.connectToHost(QHostAddress::LocalHost, port);
            QVERIFY(socket.waitForConnected(3000));
            QTest::qWait(50);
            socket.write("GET /callback?code=");
            socket.flush();
            QTest::qWait(50);
        }
        // the server and its sockets are gone, and the client sees it
        QTRY_COMPARE(socket.state(), QAbstractSocket::UnconnectedState);
    }

    void takesARequestSentInPieces()
    {
        LoopbackServer server;
        QSignalSpy spy(&server, &LoopbackServer::requested);
        const quint16 port = server.listen({0});

        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(socket.waitForConnected(3000));
        socket.write("GET /callback?co");
        socket.flush();
        QTest::qWait(50);
        QCOMPARE(spy.size(), 0);
        socket.write("de=abc HTTP/1.1\r\n\r\n");
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(spy.first().first().toUrl().query(), u"code=abc"_s);
    }

    void ignoresOtherAddresses()
    {
        LoopbackServer server;
        QSignalSpy spy(&server, &LoopbackServer::requested);
        const quint16 port = server.listen({0});

        QVERIFY(ask(port, "GET /favicon.ico HTTP/1.1\r\n\r\n").startsWith("HTTP/1.1 404"));
        QVERIFY(ask(port, "GET /callbackx?code=a HTTP/1.1\r\n\r\n").startsWith("HTTP/1.1 404"));
        QVERIFY(ask(port, "POST /callback?code=a HTTP/1.1\r\n\r\n").startsWith("HTTP/1.1 404"));
        QVERIFY(ask(port, "nonsense\r\n\r\n").startsWith("HTTP/1.1 404"));
        QCOMPARE(spy.size(), 0);
        // and still takes the real one
        QVERIFY(ask(port, "GET /callback?code=a HTTP/1.1\r\n\r\n").startsWith("HTTP/1.1 200"));
        QCOMPARE(spy.size(), 1);
    }

    void answersOnlyOnce()
    {
        LoopbackServer server;
        QSignalSpy spy(&server, &LoopbackServer::requested);
        const quint16 port = server.listen({0});
        ask(port, "GET /callback?code=a HTTP/1.1\r\n\r\nGET /callback?code=b HTTP/1.1\r\n\r\n");
        QCOMPARE(spy.size(), 1);
        QCOMPARE(spy.first().first().toUrl().query(), u"code=a"_s);
    }

    void takesTheFirstFreePort()
    {
        QTcpServer taken;
        QVERIFY(taken.listen(QHostAddress::LocalHost, 0));
        LoopbackServer server;
        // the first is in use, so it goes on to the next
        const quint16 port = server.listen({taken.serverPort(), 0});
        QVERIFY(port != 0);
        QVERIFY(port != taken.serverPort());
    }

    void saysWhenNoneAreFree()
    {
        QTcpServer taken;
        QVERIFY(taken.listen(QHostAddress::LocalHost, 0));
        LoopbackServer server;
        QCOMPARE(server.listen({taken.serverPort()}), quint16(0));
        QVERIFY(!server.isListening());
        QCOMPARE(server.listen({}), quint16(0));
    }

    void onlyListensOnLoopback()
    {
        LoopbackServer server;
        const quint16 port = server.listen({0});
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(socket.peerAddress().isLoopback());
    }

    void closes()
    {
        LoopbackServer server;
        const quint16 port = server.listen({0});
        QVERIFY(server.isListening());
        server.close();
        QVERIFY(!server.isListening());
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(!socket.waitForConnected(500));
    }
};

QTEST_GUILESS_MAIN(TestLoopbackServer)
#include "tst_loopbackserver.moc"
