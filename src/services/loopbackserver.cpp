// SPDX-License-Identifier: MIT
#include "services/loopbackserver.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Longer than a browser takes to send its first line, and the line itself is
// only ever an address with a code in it.
constexpr int longestRequest = 8192;
constexpr int quietLimit = 5000; // ms

void reply(QTcpSocket *socket, const QByteArray &status, const QByteArray &body)
{
    // whatever comes after is ignored
    socket->setProperty("answered", true);
    const QByteArray response =
        QByteArray("HTTP/1.1 ") + status +
        "\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + QByteArray::number(body.size()) +
        "\r\nConnection: close\r\n\r\n" + body;
    socket->write(response);
    socket->disconnectFromHost();
}

} // namespace

LoopbackServer::LoopbackServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &LoopbackServer::accept);
}

quint16 LoopbackServer::listen(const QList<quint16> &ports)
{
    close();
    for (const quint16 port : ports) {
        if (m_server.listen(QHostAddress::LocalHost, port))
            return m_server.serverPort();
    }
    return 0;
}

void LoopbackServer::close()
{
    m_server.close();
}

void LoopbackServer::accept()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        // a connection that never says anything is let go
        QTimer::singleShot(quietLimit, socket, [socket] { socket->disconnectFromHost(); });
    }
}

void LoopbackServer::read(QTcpSocket *socket)
{
    if (socket->property("answered").toBool())
        return;
    if (!socket->canReadLine()) {
        if (socket->bytesAvailable() > longestRequest)
            reply(socket, "431 Request Header Fields Too Large", {});
        return;
    }
    // only the first line matters: "GET /callback?code=... HTTP/1.1"
    const QByteArray line = socket->readLine(longestRequest + 1);
    const QList<QByteArray> parts = line.trimmed().split(' ');
    if (!line.endsWith('\n') || parts.size() != 3 || parts[0] != "GET") {
        reply(socket, "404 Not Found", {});
        return;
    }
    const QUrl url(u"http://127.0.0.1"_s + QString::fromLatin1(parts[1]));
    if (url.path() != u"/callback"_s) {
        reply(socket, "404 Not Found", {});
        return;
    }
    const QString message =
        m_page ? m_page(url) : u"Path of Upgrading got the answer. You can close this tab."_s;
    reply(socket, "200 OK",
          "<!doctype html><meta charset=utf-8><title>Path of Upgrading</title><p>" + message.toUtf8() +
              "</p>");
    Q_EMIT requested(url);
}

} // namespace pou
