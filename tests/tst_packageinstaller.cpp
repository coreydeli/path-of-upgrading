// SPDX-License-Identifier: MIT
#include "platform/packageinstaller.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

void write(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

QByteArray read(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestPackageInstaller : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void anAppImageIsReplacedInPlace()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"app.AppImage"_s);
        const QString update = dir.filePath(u"new.AppImage"_s);
        write(target, "old");
        write(update, "new");
        AppImageInstaller installer(target);
        QString error = u"not called"_s;
        installer.install(update, [&](const QString &e) { error = e; });
        QVERIFY(error.isEmpty());
        QCOMPARE(read(target), QByteArray("new"));
#ifndef Q_OS_WIN
        QVERIFY(QFile::permissions(target).testFlag(QFileDevice::ExeOwner));
#endif
        QVERIFY(!QFile::exists(target + u".new"_s));
        // the old one is kept beside it
        QCOMPARE(read(target + u".old"_s), QByteArray("old"));
        QVERIFY(QFile::exists(update));
        QCOMPARE(installer.after(), PackageInstaller::After::Restart);
    }

    void aSecondUpdateReplacesTheOldCopy()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"app.AppImage"_s);
        write(target, "one");
        write(dir.filePath(u"two"_s), "two");
        write(dir.filePath(u"three"_s), "three");
        AppImageInstaller installer(target);
        installer.install(dir.filePath(u"two"_s), [](const QString &) {});
        installer.install(dir.filePath(u"three"_s), [](const QString &) {});
        QCOMPARE(read(target), QByteArray("three"));
        QCOMPARE(read(target + u".old"_s), QByteArray("two"));
    }

    void aNewAppImageThatNeverConfirmsIsReplacedByTheOldOne()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"app.AppImage"_s);
        write(target, "old");
        write(dir.filePath(u"new"_s), "new");
        AppImageInstaller installer(target);
        installer.install(dir.filePath(u"new"_s), [](const QString &) {});

        // the first start of the new one: nothing to put back yet
        OldAppImage first(target);
        QVERIFY(!first.restoreIfUnconfirmed());
        first.begin();
        QVERIFY(QFile::exists(first.markerPath()));
        QCOMPARE(read(target), QByteArray("new"));

        // it didn't confirm, so the next start puts the old one back
        OldAppImage second(target);
        QVERIFY(second.restoreIfUnconfirmed());
        QCOMPARE(read(target), QByteArray("old"));
        QVERIFY(!QFile::exists(second.oldPath()));
        QVERIFY(!QFile::exists(second.markerPath()));
        // and that one starts as usual
        OldAppImage third(target);
        QVERIFY(!third.restoreIfUnconfirmed());
    }

    void aNewAppImageThatConfirmsKeepsItself()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"app.AppImage"_s);
        write(target, "old");
        write(dir.filePath(u"new"_s), "new");
        AppImageInstaller installer(target);
        installer.install(dir.filePath(u"new"_s), [](const QString &) {});
        OldAppImage first(target);
        first.begin();
        first.confirm();
        QVERIFY(!QFile::exists(first.oldPath()));
        QVERIFY(!QFile::exists(first.markerPath()));
        OldAppImage second(target);
        QVERIFY(!second.restoreIfUnconfirmed());
        QCOMPARE(read(target), QByteArray("new"));
    }

    void anAppImageWithNothingOldLeavesNoMarker()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"app.AppImage"_s);
        write(target, "current");
        OldAppImage hold(target);
        hold.begin();
        QVERIFY(!QFile::exists(hold.markerPath()));
        QVERIFY(!hold.restoreIfUnconfirmed());
        // and no AppImage at all does nothing
        OldAppImage none{QString()};
        none.begin();
        none.confirm();
        QVERIFY(!none.restoreIfUnconfirmed());
    }

    void anAppImageThatIsGoneIsReported()
    {
        QTemporaryDir dir;
        const QString update = dir.filePath(u"new.AppImage"_s);
        write(update, "new");
        AppImageInstaller installer(dir.filePath(u"missing.AppImage"_s));
        QString error;
        installer.install(update, [&](const QString &e) { error = e; });
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(dir.filePath(u"missing.AppImage"_s)));
    }

    void aMissingUpdateLeavesTheAppImageAlone()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"app.AppImage"_s);
        write(target, "old");
        AppImageInstaller installer(target);
        QString error;
        installer.install(dir.filePath(u"nothing"_s), [&](const QString &e) { error = e; });
        QVERIFY(!error.isEmpty());
        QCOMPARE(read(target), QByteArray("old"));
    }

    void theSetupRunsQuietlyAndStartsTheProgram()
    {
        QString program;
        QStringList arguments;
        SetupInstaller installer([&](const QString &p, const QStringList &a) {
            program = p;
            arguments = a;
            return true;
        });
        QString error = u"not called"_s;
        installer.install(u"C:/x/setup.exe"_s, [&](const QString &e) { error = e; });
        QVERIFY(error.isEmpty());
        QCOMPARE(program, u"C:/x/setup.exe"_s);
        QCOMPARE(arguments, (QStringList{u"/S"_s, u"/START"_s}));
        QCOMPARE(installer.after(), PackageInstaller::After::Quit);
    }

    void aSetupThatWontStartIsReported()
    {
        SetupInstaller installer([](const QString &, const QStringList &) { return false; });
        QString error;
        installer.install(u"setup.exe"_s, [&](const QString &e) { error = e; });
        QVERIFY(!error.isEmpty());
    }

    void onlySomeKindsInstallThemselves()
    {
        QVERIFY(createPackageInstaller(InstallKind::AppImage, u"/x/a.AppImage"_s));
        QVERIFY(createPackageInstaller(InstallKind::Windows, {}));
        QVERIFY(!createPackageInstaller(InstallKind::Arch, {}));
#ifndef Q_OS_WIN
        QVERIFY(createPackageInstaller(InstallKind::Debian, {}));
        QVERIFY(createPackageInstaller(InstallKind::Fedora, {}));
#endif
        QVERIFY(!createPackageInstaller(InstallKind::Source, {}));
    }
};

QTEST_GUILESS_MAIN(TestPackageInstaller)
#include "tst_packageinstaller.moc"
