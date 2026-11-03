// SPDX-License-Identifier: MIT
#pragma once

#include <QLockFile>
#include <QObject>
#include <QStringList>

class QLocalServer;
class QLocalSocket;

namespace pou {

// One copy of the program at a time, without D-Bus. The first copy holds a
// lock file and listens on a local socket, and copies started after it hand
// it their arguments there.
class LocalInstance : public QObject
{
    Q_OBJECT

public:
    // The name is the lock file's and the socket's, so it should say whose
    // they are.
    explicit LocalInstance(const QString &name, QObject *parent = nullptr);

    // Whether this is the first copy. When it isn't, the first one is handed
    // the arguments, the way QCoreApplication has them.
    bool claim(const QStringList &arguments);

Q_SIGNALS:
    // Another copy was started, with these arguments.
    void startedAgain(const QStringList &arguments);

private:
    void listen();
    void read(QLocalSocket *socket);
    bool handOver(const QStringList &arguments);

    QString m_name;
    QLockFile m_lock;
    QLocalServer *m_server = nullptr;
};

} // namespace pou
