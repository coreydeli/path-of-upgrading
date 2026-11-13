// SPDX-License-Identifier: MIT
#include "platform/localinstance.h"

#include <QDataStream>
#include <QDeadlineTimer>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace pou {
namespace {

// how long a copy keeps trying the first one, which can still be starting
constexpr auto handOverTime = 3s;
constexpr int connectTime = 500; // in milliseconds, for each try
// A copy hands over its arguments, which are a few short words, so anything
// much more than that isn't one.
constexpr qint64 mostBytes = 64 * 1024;
constexpr quint32 mostArguments = 256;

// Lets go of a client that sent what no copy would.
void drop(QLocalSocket *socket)
{
    qWarning("Something that isn't a copy of the program sent too much, and was turned away");
    socket->abort();
    socket->deleteLater();
}

} // namespace

LocalInstance::LocalInstance(const QString &name, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_lock(QDir::tempPath() + u'/' + name + u".lock"_s)
{}

void LocalInstance::setReadTime(std::chrono::milliseconds wait)
{
    m_readTime = wait;
}

bool LocalInstance::claim(const QStringList &arguments)
{
    // one left by a copy that crashed is taken over
    if (m_lock.tryLock(0)) {
        listen();
        return true;
    }
    if (!handOver(arguments))
        qWarning("Another copy is running, but it couldn't be reached");
    return false;
}

void LocalInstance::listen()
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    // the socket a copy that crashed left, which the lock says is no one's
    QLocalServer::removeServer(m_name);
    if (!m_server->listen(m_name)) {
        qWarning("Copies started later can't reach this one: %s", qUtf8Printable(m_server->errorString()));
        return;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] { read(socket); });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            // one that never sends it all would stay for good
            QTimer::singleShot(m_readTime, socket, [socket] {
                socket->abort();
                socket->deleteLater();
            });
            // what came with the connection
            read(socket);
        }
    });
}

void LocalInstance::read(QLocalSocket *socket)
{
    if (socket->bytesAvailable() > mostBytes) {
        drop(socket);
        return;
    }
    QDataStream stream(socket);
    stream.setVersion(QDataStream::Qt_6_8);
    stream.startTransaction();
    // A QStringList as handOver() writes it. Read as a list, the count would
    // be reserved before anything else is read, however large.
    quint32 count = 0;
    stream >> count;
    if (stream.status() == QDataStream::Ok && count > mostArguments) {
        drop(socket);
        return;
    }
    QStringList arguments;
    for (quint32 i = 0; i < count && stream.status() == QDataStream::Ok; ++i) {
        QString argument;
        stream >> argument;
        arguments.append(argument);
    }
    // until it has all come
    if (!stream.commitTransaction())
        return;
    socket->disconnectFromServer();
    Q_EMIT startedAgain(arguments);
}

bool LocalInstance::handOver(const QStringList &arguments)
{
    const QDeadlineTimer deadline(handOverTime);
    while (!deadline.hasExpired()) {
        QLocalSocket socket;
        socket.connectToServer(m_name);
        if (!socket.waitForConnected(connectTime)) {
            QThread::sleep(100ms);
            continue;
        }
        QDataStream stream(&socket);
        stream.setVersion(QDataStream::Qt_6_8);
        stream << arguments;
        if (!socket.waitForBytesWritten(connectTime))
            return false;
        // The first one hangs up once it has them. They're there to be read
        // even if it hasn't yet.
        socket.waitForDisconnected(connectTime);
        return true;
    }
    return false;
}

} // namespace pou
