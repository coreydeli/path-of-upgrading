// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QObject>
#include <QTcpServer>
#include <QUrl>

namespace pou {

// A tiny web server on this computer's loopback address, for the browser to
// hand the sign in's answer to. It takes one thing: a GET of /callback.
// Anything else, like the browser asking for an icon, gets a 404 and is
// ignored.
class LoopbackServer : public QObject
{
    Q_OBJECT

public:
    explicit LoopbackServer(QObject *parent = nullptr);

    // Listens on the first of the ports that's free, and says which, or zero
    // when none are. A port of zero takes any free one.
    quint16 listen(const QList<quint16> &ports);
    void close();
    bool isListening() const { return m_server.isListening(); }

Q_SIGNALS:
    // The address that was asked for, with its query.
    void requested(const QUrl &url);

private:
    void accept();
    void read(QTcpSocket *socket, QByteArray *pending);

    QTcpServer m_server;
};

} // namespace pou
