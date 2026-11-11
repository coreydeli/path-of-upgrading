// SPDX-License-Identifier: MIT
#include "app/settingsstore.h"
#include "app/updates.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "services/updatechecker.h"
#include "services/updatedownloader.h"
#include "updatetexts.h"

#include <QCryptographicHash>
#include <QFile>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Version v(const QString &text)
{
    return parseVersion(text).value_or(Version{});
}

FakeAnswer releases(const QStringList &versions)
{
    QList<QJsonObject> list;
    for (const QString &version : versions)
        list << fakeRelease(version, version.contains(u"beta"_s));
    return {.body = fakeReleases(list)};
}

// The model on a checker that has the version 0.6.0, and follows the settings
// as the app does.
struct Setup
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    FakeNetwork network;
    UpdateChecker checker{&network, v(u"0.6.0"_s)};
    FakeOpener opener;
    UpdateDownloader downloader{&network, dir.filePath(u"updates"_s)};
    Updates updates{&checker, &settings, &opener, &downloader};

    Setup()
    {
        // whatever this test machine is
        updates.setInstallKind(InstallKind::Source);
        QObject::connect(&settings, &SettingsStore::changed, &settings,
                         [this] { checker.follow(settings.settings().updates); });
        checker.follow(settings.settings().updates);
    }

    void look(const QStringList &versions)
    {
        network.answers = {releases(versions)};
        updates.checkNow();
        QVERIFY(QTest::qWaitFor([&] { return !updates.isChecking(); }, 5000));
    }
};

const QByteArray package = "pretend this is a package";

// SHA256SUMS for the release's file of this kind.
FakeAnswer sumsFor(const QString &version, InstallKind kind)
{
    const Release release = readReleases(fakeReleases({fakeRelease(version)})).first();
    const QByteArray sum = QCryptographicHash::hash(package, QCryptographicHash::Sha256).toHex();
    return {.body = sum + "  " + assetFor(release, kind)->name.toUtf8() + "\n"};
}

// Keeps what it's asked to install, and says how it went.
class FakeInstaller : public PackageInstaller
{
public:
    After after() const override { return m_after; }
    bool fallsBackToCommand() const override { return fallback; }
    void install(const QString &file, const Done &done) override
    {
        installed << file;
        done(error);
    }

    QStringList installed;
    QString error;
    bool fallback = false;
    After m_after = After::Restart;
};

} // namespace

class TestUpdatesModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLoggingCategory::setFilterRules(u"pathofupgrading.updates.warning=false"_s); }

    void sayNothingBeforeTheFirstLook()
    {
        Setup s;
        QCOMPARE(s.updates.version(), u"0.6.0"_s);
        QVERIFY(s.updates.status().isEmpty());
        QVERIFY(!s.updates.isPending());
        QVERIFY(s.updates.newVersion().isEmpty());
        QVERIFY(s.updates.notes().isEmpty());
    }

    void offersANewerRelease()
    {
        Setup s;
        QSignalSpy offered(&s.updates, &Updates::offered);
        s.network.answers = {releases({u"0.7.0"_s})};
        s.updates.checkNow();
        QCOMPARE(s.updates.status(), u"Looking for updates."_s);
        QVERIFY(s.updates.isChecking());
        QVERIFY(QTest::qWaitFor([&] { return !s.updates.isChecking(); }, 5000));
        QCOMPARE(s.updates.status(), u"Version 0.7.0 is out."_s);
        QVERIFY(s.updates.isPending());
        QCOMPARE(s.updates.newVersion(), u"0.7.0"_s);
        QCOMPARE(s.updates.notes(), u"What is new in 0.7.0."_s);
        QCOMPARE(offered.size(), 1);
    }

    void saysWhenYouHaveTheNewest()
    {
        Setup s;
        s.look({u"0.6.0"_s});
        QCOMPARE(s.updates.status(), u"You have the newest version."_s);
        QVERIFY(!s.updates.isPending());
    }

    void saysWhyItCouldNotLook()
    {
        Setup s;
        s.network.answers = {FakeAnswer{.status = 403}};
        s.updates.checkNow();
        QVERIFY(QTest::qWaitFor([&] { return !s.updates.isChecking(); }, 5000));
        QVERIFY(!s.updates.status().isEmpty());
        QVERIFY(!s.updates.status().startsWith(u"Version"_s));
        QVERIFY(!s.updates.isPending());
    }

    void betasAreYourChoice()
    {
        Setup s;
        QCOMPARE(s.updates.includesBetas(), isBetaBuild());
        s.updates.setIncludeBetas(false);
        s.look({u"0.7.0-beta.1"_s});
        QVERIFY(!s.updates.isPending());
        s.updates.setIncludeBetas(true);
        QVERIFY(s.settings.settings().updates.includeBetas);
        QVERIFY(s.updates.isPending());
        QCOMPARE(s.updates.newVersion(), u"0.7.0-beta.1"_s);
    }

    void checkingOnLaunchIsSaved()
    {
        Setup s;
        QVERIFY(s.updates.checksOnLaunch());
        s.updates.setCheckOnLaunch(false);
        QVERIFY(!s.settings.settings().updates.checkOnLaunch);
        QVERIFY(!s.updates.checksOnLaunch());
    }

    void laterLeavesItForTheNextStart()
    {
        Setup s;
        s.look({u"0.7.0"_s});
        s.updates.later();
        QVERIFY(!s.updates.isPending());
        QVERIFY(s.settings.settings().updates.skipped.isEmpty());
        // asking again offers it again
        QSignalSpy offered(&s.updates, &Updates::offered);
        s.look({u"0.7.0"_s});
        QVERIFY(s.updates.isPending());
        QCOMPARE(offered.size(), 1);
    }

    void aNewerOneAfterLaterIsOffered()
    {
        Setup s;
        s.look({u"0.7.0"_s});
        s.updates.later();
        s.look({u"0.7.0"_s, u"0.8.0"_s});
        QVERIFY(s.updates.isPending());
        QCOMPARE(s.updates.newVersion(), u"0.8.0"_s);
    }

    void skippingIsKeptAndOnlyForThatVersion()
    {
        Setup s;
        s.look({u"0.7.0"_s});
        s.updates.skip();
        QCOMPARE(s.settings.settings().updates.skipped, u"0.7.0"_s);
        QVERIFY(!s.updates.isPending());
        // and a newer one still comes
        s.network.answers = {releases({u"0.7.0"_s, u"0.8.0"_s})};
        s.checker.check();
        QVERIFY(QTest::qWaitFor([&] { return !s.updates.isChecking(); }, 5000));
        QCOMPARE(s.updates.newVersion(), u"0.8.0"_s);
        QVERIFY(s.updates.isPending());
    }

    void checkNowOffersASkippedVersionAgain()
    {
        Setup s;
        s.look({u"0.7.0"_s});
        s.updates.skip();
        QVERIFY(!s.updates.isPending());
        s.look({u"0.7.0"_s});
        QVERIFY(s.settings.settings().updates.skipped.isEmpty());
        QVERIFY(s.updates.isPending());
    }

    void updateNowOpensTheReleasePage()
    {
        Setup s;
        s.updates.updateNow();
        QVERIFY(s.opener.opened.isEmpty());
        s.look({u"0.7.0"_s});
        s.updates.updateNow();
        QCOMPARE(s.opener.opened, QList<QUrl>{QUrl(releaseBase + u"/tag/v0.7.0"_s)});
        QVERIFY(!s.updates.isPending());
    }

    void downloadsAndShowsTheCommandForAPackage()
    {
        Setup s;
        s.updates.setInstallKind(InstallKind::Debian);
        s.look({u"0.7.0"_s});
        s.network.answers = {sumsFor(u"0.7.0"_s, InstallKind::Debian), {.body = package}};
        s.updates.updateNow();
        QCOMPARE(s.updates.stage(), UpdateStage::Downloading);
        QVERIFY(QTest::qWaitFor([&] { return s.updates.stage() != UpdateStage::Downloading; }, 5000));
        QCOMPARE(s.updates.stage(), UpdateStage::Manual);
        QVERIFY(s.updates.command().startsWith(u"sudo apt install '"_s));
        QVERIFY(s.updates.command().contains(u"/updates/0.7.0/"_s));
        // it never opens the page, and doesn't offer it again
        QVERIFY(s.opener.opened.isEmpty());
        QVERIFY(!s.updates.isPending());

        s.updates.copyCommand();
        QCOMPARE(s.opener.copied, QStringList{s.updates.command()});
        s.updates.openFolder();
        QCOMPARE(s.opener.opened.size(), 1);
        QVERIFY(s.opener.opened.at(0).isLocalFile());
        QVERIFY(s.opener.opened.at(0).toLocalFile().endsWith(u"/updates/0.7.0"_s));

        s.updates.dismiss();
        QCOMPARE(s.updates.stage(), UpdateStage::Idle);
        QVERIFY(s.updates.command().isEmpty());
    }

    void installsByItselfWhenItCan()
    {
        Setup s;
        s.updates.setInstallKind(InstallKind::AppImage);
        auto installer = std::make_unique<FakeInstaller>();
        FakeInstaller *fake = installer.get();
        QList<PackageInstaller::After> finished;
        s.updates.setInstaller(std::move(installer), [&](PackageInstaller::After a) { finished << a; });
        s.look({u"0.7.0"_s});
        s.network.answers = {sumsFor(u"0.7.0"_s, InstallKind::AppImage), {.body = package}};
        s.updates.updateNow();
        QVERIFY(QTest::qWaitFor([&] { return !finished.isEmpty(); }, 5000));
        QCOMPARE(fake->installed.size(), 1);
        QVERIFY(fake->installed.at(0).endsWith(u"x86_64.AppImage"_s));
        QCOMPARE(finished, QList<PackageInstaller::After>{PackageInstaller::After::Restart});
        QVERIFY(s.updates.command().isEmpty());
    }

    void anInstallThatFailsSaysSoAndKeepsRunning()
    {
        Setup s;
        s.updates.setInstallKind(InstallKind::AppImage);
        auto installer = std::make_unique<FakeInstaller>();
        installer->error = u"Couldn't replace it."_s;
        bool finished = false;
        s.updates.setInstaller(std::move(installer), [&](PackageInstaller::After) { finished = true; });
        s.look({u"0.7.0"_s});
        s.network.answers = {sumsFor(u"0.7.0"_s, InstallKind::AppImage), {.body = package}};
        s.updates.updateNow();
        QVERIFY(QTest::qWaitFor([&] { return s.updates.stage() == UpdateStage::Failed; }, 5000));
        QCOMPARE(s.updates.installError(), u"Couldn't replace it."_s);
        QVERIFY(!finished);
        s.updates.dismiss();
        QCOMPARE(s.updates.stage(), UpdateStage::Idle);
    }

    void aPackageInstallsAndOffersARestart()
    {
        Setup s;
        s.updates.setInstallKind(InstallKind::Debian);
        auto installer = std::make_unique<FakeInstaller>();
        installer->fallback = true;
        installer->m_after = PackageInstaller::After::Offer;
        QList<PackageInstaller::After> finished;
        s.updates.setInstaller(std::move(installer), [&](PackageInstaller::After a) { finished << a; });
        s.look({u"0.7.0"_s});
        s.network.answers = {sumsFor(u"0.7.0"_s, InstallKind::Debian), {.body = package}};
        s.updates.updateNow();
        QVERIFY(QTest::qWaitFor([&] { return s.updates.stage() == UpdateStage::Installed; }, 5000));
        // it waits for you
        QVERIFY(finished.isEmpty());
        QVERIFY(s.updates.command().isEmpty());
        s.updates.restart();
        QCOMPARE(finished, QList<PackageInstaller::After>{PackageInstaller::After::Restart});
    }

    void aPackageThatWontInstallShowsTheCommandAndWhy()
    {
        Setup s;
        s.updates.setInstallKind(InstallKind::Fedora);
        auto installer = std::make_unique<FakeInstaller>();
        installer->fallback = true;
        installer->error = u"The install was cancelled."_s;
        s.updates.setInstaller(std::move(installer), [](PackageInstaller::After) {});
        s.look({u"0.7.0"_s});
        s.network.answers = {sumsFor(u"0.7.0"_s, InstallKind::Fedora), {.body = package}};
        s.updates.updateNow();
        QVERIFY(QTest::qWaitFor([&] { return s.updates.stage() == UpdateStage::Manual; }, 5000));
        QCOMPARE(s.updates.installError(), u"The install was cancelled."_s);
        QVERIFY(s.updates.command().startsWith(u"sudo dnf install '"_s));
        // and restarting isn't offered
        s.updates.restart();
        QCOMPARE(s.updates.stage(), UpdateStage::Manual);
    }

    void aBadDownloadFails()
    {
        Setup s;
        s.updates.setInstallKind(InstallKind::Debian);
        s.look({u"0.7.0"_s});
        s.network.answers = {sumsFor(u"0.7.0"_s, InstallKind::Debian), {.body = "something else"}};
        s.updates.updateNow();
        QVERIFY(QTest::qWaitFor([&] { return s.updates.stage() == UpdateStage::Failed; }, 5000));
        QCOMPARE(s.updates.installError(),
                 u"The download didn't match its checksum, so it was thrown away."_s);
        QVERIFY(s.updates.command().isEmpty());
    }

    void cancellingStopsTheDownload()
    {
        Setup s;
        s.updates.setInstallKind(InstallKind::Debian);
        s.look({u"0.7.0"_s});
        s.network.answers = {sumsFor(u"0.7.0"_s, InstallKind::Debian), FakeAnswer{.never = true}};
        s.updates.updateNow();
        QTRY_VERIFY(s.network.sent.size() >= 3);
        s.updates.dismiss();
        QCOMPARE(s.updates.stage(), UpdateStage::Idle);
        QVERIFY(!s.downloader.isRunning());
    }

    void aKindWithNoFileOpensThePage()
    {
        Setup s;
        s.updates.setInstallKind(InstallKind::Debian);
        // a release with no files at all
        QJsonObject bare = fakeRelease(u"0.7.0"_s);
        bare[u"assets"_s] = QJsonArray();
        s.network.answers = {FakeAnswer{.body = fakeReleases({bare})}};
        s.updates.checkNow();
        QVERIFY(QTest::qWaitFor([&] { return !s.updates.isChecking(); }, 5000));
        s.updates.updateNow();
        QCOMPARE(s.updates.stage(), UpdateStage::Idle);
        QCOMPARE(s.opener.opened, QList<QUrl>{QUrl(releaseBase + u"/tag/v0.7.0"_s)});
    }

    void nothingToDoWithoutARelease()
    {
        Setup s;
        s.updates.later();
        s.updates.skip();
        QVERIFY(s.settings.settings().updates.skipped.isEmpty());
        QVERIFY(s.opener.opened.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestUpdatesModel)
#include "tst_updatesmodel.moc"
