// SPDX-License-Identifier: MIT
#include "platform/packagekitinstaller.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

using namespace Qt::Literals::StringLiterals;

namespace pou {

namespace {

const QString rootPath = u"/org/freedesktop/PackageKit"_s;
const QString rootInterface = u"org.freedesktop.PackageKit"_s;
const QString transactionInterface = u"org.freedesktop.PackageKit.Transaction"_s;

// PackageKit's exit codes for a transaction
constexpr uint exitSuccess = 1;
constexpr uint exitCancelled = 3;

} // namespace

// One install: makes the transaction, listens to it, and asks for the file.
class PackageKitRun : public QObject
{
    Q_OBJECT

public:
    PackageKitRun(const QDBusConnection &bus, const QString &service, const QString &file,
                  const PackageInstaller::Done &done)
        : m_bus(bus)
        , m_service(service)
        , m_file(file)
        , m_done(done)
    {
        QDBusMessage create =
            QDBusMessage::createMethodCall(m_service, rootPath, rootInterface, u"CreateTransaction"_s);
        auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(create), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, &PackageKitRun::created);
    }

private Q_SLOTS:
    void created(QDBusPendingCallWatcher *watcher)
    {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            // PackageKit not being on the bus is the usual reason
            finish(u"PackageKit isn't available here: %1"_s.arg(reply.error().message()));
            return;
        }
        const QString path = reply.value().path();
        m_bus.connect(m_service, path, transactionInterface, u"ErrorCode"_s, this,
                      SLOT(errorCame(uint, QString)));
        m_bus.connect(m_service, path, transactionInterface, u"Finished"_s, this, SLOT(finished(uint, uint)));
        // no flags, which leaves "only trusted" off, as this isn't from a repository
        QDBusMessage install =
            QDBusMessage::createMethodCall(m_service, path, transactionInterface, u"InstallFiles"_s);
        install << uint(0) << QStringList{m_file};
        auto *started = new QDBusPendingCallWatcher(m_bus.asyncCall(install), this);
        connect(started, &QDBusPendingCallWatcher::finished, this, &PackageKitRun::started);
    }

    void started(QDBusPendingCallWatcher *watcher)
    {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError())
            finish(u"PackageKit wouldn't install it: %1"_s.arg(reply.error().message()));
    }

    void errorCame(uint, const QString &details) { m_reason = details; }

    void finished(uint exitCode, uint)
    {
        if (exitCode == exitSuccess) {
            finish({});
            return;
        }
        if (!m_reason.isEmpty())
            finish(u"PackageKit said: %1"_s.arg(m_reason));
        else if (exitCode == exitCancelled)
            finish(u"The install was cancelled."_s);
        else
            finish(u"PackageKit couldn't install it."_s);
    }

private:
    void finish(const QString &error)
    {
        if (m_over)
            return;
        m_over = true;
        // the caller may delete things, so it goes after this is out of the way
        const PackageInstaller::Done done = m_done;
        deleteLater();
        done(error);
    }

    QDBusConnection m_bus;
    QString m_service;
    QString m_file;
    PackageInstaller::Done m_done;
    QString m_reason;
    bool m_over = false;
};

PackageKitInstaller::PackageKitInstaller()
    : m_service(rootInterface)
{}

PackageKitInstaller::PackageKitInstaller(const QDBusConnection &bus, const QString &service)
    : m_bus(bus)
    , m_service(service)
{}

PackageKitInstaller::~PackageKitInstaller()
{
    delete m_running;
}

void PackageKitInstaller::install(const QString &file, const Done &done)
{
    if (m_running) {
        done(u"An install is already running."_s);
        return;
    }
    m_running = new PackageKitRun(m_bus.value_or(QDBusConnection::systemBus()), m_service, file, done);
}

} // namespace pou

#include "packagekitinstaller.moc"
