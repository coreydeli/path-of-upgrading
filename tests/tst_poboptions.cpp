// SPDX-License-Identifier: MIT
#include "app/poboptions.h"
#include "app/settingsstore.h"
#include "fakeopener.h"
#include "fakepob.h"
#include "services/pobengine.h"

#include <QDir>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <limits>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

struct Options
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    PobEngine pob;
    // a computer with flatpak and LuaJIT, and no Path of Building
    PobPlaces places{
        .home = dir.path(),
        .dataHome = dir.filePath(u"data"_s),
        .flatpak = u"/usr/bin/flatpak"_s,
        .luajit = u"/usr/bin/luajit"_s,
    };
    FakeOpener opener;
    PobOptions options{&settings, &pob, places, &opener};

    const PobSettings &saved() const { return settings.settings().pob; }
};

} // namespace

class TestPobOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void readsTheSettings()
    {
        Options o;
        Settings settings;
        PobSettings &pob = settings.pob;
        pob.enabled = false;
        pob.location = u"/games/pob"_s;
        pob.damageWeight = 0.25;
        pob.priorities = {
            .resistsFirst = false,
            .chaos = true,
            .keepDamage = false,
            .keepDefence = false,
            .spirit = false,
            .attributes = false,
        };
        QVERIFY(o.settings.set(settings));

        QCOMPARE(o.options.isEnabled(), false);
        QCOMPARE(o.options.location(), u"/games/pob"_s);
        QCOMPARE(o.options.damageWeight(), 0.25);
        QCOMPARE(o.options.resistsFirst(), false);
        QCOMPARE(o.options.chaos(), true);
        QCOMPARE(o.options.keepDamage(), false);
        QCOMPARE(o.options.keepDefence(), false);
        QCOMPARE(o.options.spirit(), false);
        QCOMPARE(o.options.attributes(), false);
    }

    void savesWhatYouChange()
    {
        Options o;
        QSignalSpy changed(&o.options, &PobOptions::changed);
        o.options.setEnabled(false);
        o.options.setLocation(u"/games/pob"_s);
        o.options.setDamageWeight(0.8);
        o.options.setResistsFirst(false);
        o.options.setChaos(true);
        o.options.setKeepDamage(false);
        o.options.setKeepDefence(false);
        o.options.setSpirit(false);
        o.options.setAttributes(false);
        QCOMPARE(changed.size(), 9);

        PobSettings pob;
        pob.enabled = false;
        pob.location = u"/games/pob"_s;
        pob.damageWeight = 0.8;
        pob.priorities = {
            .resistsFirst = false,
            .chaos = true,
            .keepDamage = false,
            .keepDefence = false,
            .spirit = false,
            .attributes = false,
        };
        QCOMPARE(o.saved(), pob);
        QCOMPARE(SettingsStore(o.dir.filePath(u"settings.conf"_s)).settings().pob, pob);

        o.options.setChaos(true);
        QCOMPARE(changed.size(), 9);
    }

    void andBack()
    {
        Options o;
        o.options.setEnabled(false);
        o.options.setResistsFirst(false);
        o.options.setChaos(true);
        o.options.setKeepDamage(false);
        o.options.setKeepDefence(false);
        o.options.setSpirit(false);
        o.options.setAttributes(false);
        o.options.setEnabled(true);
        o.options.setResistsFirst(true);
        o.options.setChaos(false);
        o.options.setKeepDamage(true);
        o.options.setKeepDefence(true);
        o.options.setSpirit(true);
        o.options.setAttributes(true);
        QCOMPARE(o.saved(), PobSettings());
    }

    void typedLocation()
    {
        Options o;
        o.options.setLocation(u"  /games/pob "_s);
        QCOMPARE(o.saved().location, u"/games/pob"_s);
        o.options.setLocation(u" "_s);
        QCOMPARE(o.saved().location, QString());
    }

    void pickedFolder()
    {
        Options o;
        o.options.setLocationUrl(QUrl::fromLocalFile(u"/games/pob"_s));
        QCOMPARE(o.saved().location, u"/games/pob"_s);
        // only folders on this computer
        o.options.setLocationUrl(QUrl(u"https://example.com/pob"_s));
        o.options.setLocationUrl({});
        QCOMPARE(o.saved().location, u"/games/pob"_s);
    }

    void damageWeight_data()
    {
        QTest::addColumn<double>("typed");
        QTest::addColumn<double>("kept");
        QTest::newRow("some") << 0.3 << 0.3;
        QTest::newRow("none") << 0.0 << 0.0;
        QTest::newRow("too little") << -0.5 << 0.0;
        QTest::newRow("all") << 1.0 << 1.0;
        QTest::newRow("too much") << 2.0 << 1.0;
        QTest::newRow("not a number") << std::numeric_limits<double>::quiet_NaN() << 0.4;
    }

    void damageWeight()
    {
        QFETCH(double, typed);
        QFETCH(double, kept);
        Options o;
        o.options.setDamageWeight(0.4);
        o.options.setDamageWeight(typed);
        QCOMPARE(o.saved().damageWeight, kept);
    }

    void found()
    {
        Options o;
        QCOMPARE(o.options.installedAt(), QString());
        QCOMPARE(o.options.headline(), u"Path of Building wasn't found."_s);

        QSignalSpy changed(&o.options, &PobOptions::changed);
        o.pob.setInstall({.folder = u"/data/RustyPathOfBuilding2"_s,
                          .program = u"luajit"_s,
                          .userFolder = u"/data/RustyPathOfBuilding2/userdata"_s});
        QVERIFY(!changed.isEmpty());
        QCOMPARE(o.options.headline(), u"Using the Path of Building in your data folder."_s);
        // with backslashes on Windows
        QCOMPARE(o.options.installedAt(), QDir::toNativeSeparators(u"/data/RustyPathOfBuilding2"_s));
        QCOMPARE(o.options.buildsAt(),
                 QDir::toNativeSeparators(u"/data/RustyPathOfBuilding2/userdata/Builds"_s));
        QVERIFY(o.options.steps().isEmpty());
        QVERIFY(!o.options.offersFlatpak());

        changed.clear();
        o.pob.setInstall({.folder = u"/flatpak/pob"_s, .program = u"flatpak"_s, .flatpak = true});
        QVERIFY(!changed.isEmpty());
        QCOMPARE(o.options.headline(), u"Using Path of Building from Flathub."_s);
        QVERIFY(o.options.details().first().contains(u"hidden folder"));
    }

    void notFound()
    {
        Options o;
        o.pob.setInstall({.folder = u"/games/pob"_s, .program = u"luajit"_s});
        QSignalSpy changed(&o.options, &PobOptions::changed);
        o.pob.setUnavailable(u"There's no Path of Building."_s);
        QVERIFY(!changed.isEmpty());
        QCOMPARE(o.options.headline(), u"Path of Building wasn't found."_s);
        QCOMPARE(o.options.installedAt(), QString());
        QCOMPARE(o.options.steps().size(), 3);
        QVERIFY(o.options.offersFlatpak());
        QCOMPARE(o.options.installCommand(),
                 u"flatpak install flathub community.pathofbuilding.PathOfBuilding"_s);
        QCOMPARE(o.options.places().size(), 4);
    }

    void aFolderThatIsNoGood()
    {
        Options o;
        const QString empty = o.dir.filePath(u"empty"_s);
        QVERIFY(QDir().mkpath(empty));
        QVERIFY(!o.options.folderProblem());
        QSignalSpy changed(&o.options, &PobOptions::changed);
        o.options.setLocation(empty);
        QVERIFY(!changed.isEmpty());
        QVERIFY(o.options.folderProblem());
        QCOMPARE(o.options.headline(), u"There's no Path of Building in the folder you chose."_s);
        QVERIFY(o.options.folderNote().contains(u"HeadlessWrapper.lua"));
        QCOMPARE(o.options.places().size(), 1);

        o.options.useUsualPlaces();
        QCOMPARE(o.options.location(), QString());
        QVERIFY(!o.options.folderProblem());
        QCOMPARE(o.options.headline(), u"Path of Building wasn't found."_s);
        QCOMPARE(o.saved().location, QString());
    }

    void couldNotStart()
    {
        FakePob fake;
        Options o;
        o.pob.setInstall(fake.install({u"--fail"_s}));
        o.pob.setBuild(u"<PathOfBuilding2><Build level=\"90\" className=\"Sorceress\"/></PathOfBuilding2>"_s);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"^The fake didn't start\\."_s));
        QSignalSpy changed(&o.options, &PobOptions::changed);
        const std::unique_ptr<PobReply> reply(o.pob.evaluate({}));
        QSignalSpy finished(reply.get(), &PobReply::finished);
        QVERIFY(reply->isFinished() || finished.wait());
        QCOMPARE(o.pob.state(), PobEngine::State::Failed);
        QVERIFY(!changed.isEmpty());
        QCOMPARE(o.options.headline(), u"Path of Building was found, but it wouldn't start."_s);
        QVERIFY(o.options.details().contains(u"The fake didn't start."_s));
        QCOMPARE(o.options.steps().size(), 2);
        // it's still the one it uses
        QVERIFY(!o.options.installedAt().isEmpty());
    }

    void opensAndCopiesOnlyWhenAsked()
    {
        Options o;
        QVERIFY(o.opener.opened.isEmpty());
        QVERIFY(o.opener.copied.isEmpty());
        o.options.openDiscover();
        o.options.openFlathub();
        o.options.openInstaller();
        o.options.copyInstallCommand();
        QCOMPARE(o.opener.opened,
                 (QList<QUrl>{
                     QUrl(u"appstream://community.pathofbuilding.PathOfBuilding"_s),
                     QUrl(u"https://flathub.org/apps/community.pathofbuilding.PathOfBuilding"_s),
                     QUrl(u"https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2/releases"_s)}));
        QCOMPARE(o.opener.copied,
                 QStringList{u"flatpak install flathub community.pathofbuilding.PathOfBuilding"_s});
    }

    void withoutAnOpener()
    {
        QTemporaryDir dir;
        SettingsStore settings(dir.filePath(u"settings.conf"_s));
        PobEngine pob;
        PobOptions options(&settings, &pob, {}, nullptr);
        options.openDiscover();
        options.openFlathub();
        options.openInstaller();
        options.copyInstallCommand();
    }
};

QTEST_GUILESS_MAIN(TestPobOptions)
#include "tst_poboptions.moc"
