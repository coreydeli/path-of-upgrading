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
        QVERIFY(QFile::exists(update));
        QCOMPARE(installer.after(), PackageInstaller::After::Restart);
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
        QVERIFY(!createPackageInstaller(InstallKind::Debian, {}));
        QVERIFY(!createPackageInstaller(InstallKind::Fedora, {}));
        QVERIFY(!createPackageInstaller(InstallKind::Source, {}));
    }
};

QTEST_GUILESS_MAIN(TestPackageInstaller)
#include "tst_packageinstaller.moc"
