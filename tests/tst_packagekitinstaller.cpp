// SPDX-License-Identifier: MIT
#include "platform/packagekitinstaller.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusObjectPath>
#include <QTest>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString fakeService = u"test.pou.FakePackageKit"_s;

} // namespace

// What a PackageKit daemon does, for one transaction, on the private test bus.
// It never installs anything.
class FakeTransaction : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PackageKit.Transaction")

public:
    // what it does once it's asked to install
    enum class Then { Finish, Nothing, Drop };

    Then then = Then::Finish;
    uint exitCode = 1;
    QString errorDetails;
    bool refuse = false;
    uint flagsSeen = 99;
    QStringList filesSeen;

public Q_SLOTS:
    Q_SCRIPTABLE void InstallFiles(uint flags, const QStringList &files)
    {
        flagsSeen = flags;
        filesSeen = files;
        QTimer::singleShot(0, this, [this] {
            if (then == Then::Nothing)
                return;
            if (then == Then::Drop) {
                Q_EMIT Destroy();
                return;
            }
            if (!errorDetails.isEmpty())
                Q_EMIT ErrorCode(5, errorDetails);
            Q_EMIT Finished(exitCode, 1);
        });
    }

Q_SIGNALS:
    Q_SCRIPTABLE void ErrorCode(uint code, const QString &details);
    Q_SCRIPTABLE void Finished(uint exitCode, uint runtime);
    Q_SCRIPTABLE void Destroy();
};

class FakeDaemon : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PackageKit")

public:
    int created = 0;

public Q_SLOTS:
    Q_SCRIPTABLE QDBusObjectPath CreateTransaction()
    {
        ++created;
        return QDBusObjectPath(u"/1_fake"_s);
    }
};

class TestPackageKitInstaller : public QObject
{
    Q_OBJECT

    QDBusConnection m_server{u"unconnected"_s};
    FakeDaemon m_daemon;
    FakeTransaction m_transaction;

    // Installs the file, and waits for it to say how it went.
    QString run(const QString &service = fakeService)
    {
        PackageKitInstaller installer(QDBusConnection::sessionBus(), service);
        QString error = u"not called"_s;
        bool called = false;
        installer.install(u"/tmp/x/pkg.deb"_s, [&](const QString &e) {
            error = e;
            called = true;
        });
        if (!QTest::qWaitFor([&] { return called; }, 5000))
            return u"timed out"_s;
        return error;
    }

private Q_SLOTS:
    void initTestCase()
    {
        // the private bus the tests run on, with nothing else on it
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("There's no session bus to test on.");
        m_server = QDBusConnection::connectToBus(QDBusConnection::SessionBus, u"fake-packagekit"_s);
        QVERIFY(m_server.registerService(fakeService));
        QVERIFY(m_server.registerObject(u"/org/freedesktop/PackageKit"_s, &m_daemon,
                                        QDBusConnection::ExportScriptableSlots));
        QVERIFY(m_server.registerObject(u"/1_fake"_s, &m_transaction,
                                        QDBusConnection::ExportScriptableSlots |
                                            QDBusConnection::ExportScriptableSignals));
    }

    void init()
    {
        m_transaction.then = FakeTransaction::Then::Finish;
        m_transaction.exitCode = 1;
        m_transaction.errorDetails.clear();
        m_transaction.filesSeen.clear();
        m_transaction.flagsSeen = 99;
    }

    void installsTheFileUntrusted()
    {
        QVERIFY(run().isEmpty());
        QCOMPARE(m_transaction.filesSeen, QStringList{u"/tmp/x/pkg.deb"_s});
        // no flags, so "only trusted" is off
        QCOMPARE(m_transaction.flagsSeen, 0u);
    }

    void offersARestartAndFallsBackToTheCommand()
    {
        PackageKitInstaller installer;
        QCOMPARE(installer.after(), PackageInstaller::After::Offer);
        QVERIFY(installer.fallsBackToCommand());
    }

    void aFailureSaysWhatPackageKitSaid()
    {
        m_transaction.exitCode = 2;
        m_transaction.errorDetails = u"Dependency problem."_s;
        QCOMPARE(run(), u"PackageKit said: Dependency problem."_s);
    }

    void aCancelledInstallSaysSo()
    {
        m_transaction.exitCode = 3;
        QCOMPARE(run(), u"The install was cancelled."_s);
    }

    void aFailureWithNoReasonSaysSo()
    {
        m_transaction.exitCode = 2;
        QCOMPARE(run(), u"PackageKit couldn't install it."_s);
    }

    void packageKitStoppingSaysSo()
    {
        m_transaction.then = FakeTransaction::Then::Nothing;
        PackageKitInstaller installer(QDBusConnection::sessionBus(), fakeService);
        QString error = u"not called"_s;
        installer.install(u"/tmp/x/pkg.deb"_s, [&](const QString &e) { error = e; });
        QVERIFY(QTest::qWaitFor([&] { return !m_transaction.filesSeen.isEmpty(); }, 5000));
        // as when it crashes
        QVERIFY(m_server.unregisterService(fakeService));
        const bool ended = QTest::qWaitFor([&] { return error != u"not called"_s; }, 5000);
        QVERIFY(m_server.registerService(fakeService));
        QVERIFY(ended);
        QCOMPARE(error, u"PackageKit stopped before the install was done."_s);
    }

    void aTransactionDroppedUnfinishedSaysSo()
    {
        m_transaction.then = FakeTransaction::Then::Drop;
        QCOMPARE(run(), u"PackageKit stopped before the install was done."_s);
    }

    void aSecondInstallCanStartOnceOneHasEnded()
    {
        m_transaction.then = FakeTransaction::Then::Drop;
        PackageKitInstaller installer(QDBusConnection::sessionBus(), fakeService);
        int calls = 0;
        installer.install(u"/tmp/x/pkg.deb"_s, [&](const QString &) { ++calls; });
        QVERIFY(QTest::qWaitFor([&] { return calls == 1; }, 5000));
        m_transaction.then = FakeTransaction::Then::Finish;
        QString error = u"not called"_s;
        installer.install(u"/tmp/x/pkg.deb"_s, [&](const QString &e) { error = e; });
        QVERIFY(QTest::qWaitFor([&] { return error != u"not called"_s; }, 5000));
        QVERIFY(error.isEmpty());
    }

    void noPackageKitOnTheBusSaysSo()
    {
        const QString error = run(u"test.pou.NotThere"_s);
        QVERIFY(error.startsWith(u"PackageKit isn't available here: "_s));
    }
};

QTEST_GUILESS_MAIN(TestPackageKitInstaller)
#include "tst_packagekitinstaller.moc"
