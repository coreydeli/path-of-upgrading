// SPDX-License-Identifier: MIT
#include "app/poboptions.h"
#include "app/settingsstore.h"
#include "fakepob.h"
#include "services/pobengine.h"

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
    PobOptions options{&settings, &pob};

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
        o.options.setSpirit(false);
        o.options.setAttributes(false);
        QCOMPARE(changed.size(), 8);

        PobSettings pob;
        pob.enabled = false;
        pob.location = u"/games/pob"_s;
        pob.damageWeight = 0.8;
        pob.priorities = {
            .resistsFirst = false,
            .chaos = true,
            .keepDamage = false,
            .spirit = false,
            .attributes = false,
        };
        QCOMPARE(o.saved(), pob);
        QCOMPARE(SettingsStore(o.dir.filePath(u"settings.conf"_s)).settings().pob, pob);

        o.options.setChaos(true);
        QCOMPARE(changed.size(), 8);
    }

    void andBack()
    {
        Options o;
        o.options.setEnabled(false);
        o.options.setResistsFirst(false);
        o.options.setChaos(true);
        o.options.setKeepDamage(false);
        o.options.setSpirit(false);
        o.options.setAttributes(false);
        o.options.setEnabled(true);
        o.options.setResistsFirst(true);
        o.options.setChaos(false);
        o.options.setKeepDamage(true);
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
        QCOMPARE(o.options.found(), QString());
        QCOMPARE(o.options.problem(), u"There's no Path of Building to run."_s);

        QSignalSpy changed(&o.options, &PobOptions::changed);
        o.pob.setInstall({.folder = u"/games/pob"_s, .program = u"luajit"_s});
        QVERIFY(!changed.isEmpty());
        QCOMPARE(o.options.found(), u"Using Path of Building in /games/pob."_s);
        QCOMPARE(o.options.problem(), QString());

        changed.clear();
        o.pob.setInstall({.folder = u"/flatpak/pob"_s, .program = u"flatpak"_s, .flatpak = true});
        QVERIFY(!changed.isEmpty());
        QCOMPARE(o.options.found(), u"Using the Path of Building Flatpak."_s);
    }

    void notFound()
    {
        Options o;
        o.pob.setInstall({.folder = u"/games/pob"_s, .program = u"luajit"_s});
        QSignalSpy changed(&o.options, &PobOptions::changed);
        o.pob.setUnavailable(u"There's no Path of Building."_s);
        QVERIFY(!changed.isEmpty());
        QCOMPARE(o.options.found(), QString());
        QCOMPARE(o.options.problem(), u"There's no Path of Building."_s);
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
        QCOMPARE(o.options.problem(), u"The fake didn't start."_s);
        // it's still the one it uses
        QVERIFY(!o.options.found().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPobOptions)
#include "tst_poboptions.moc"
