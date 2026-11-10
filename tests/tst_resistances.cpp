// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/resistances.h"
#include "core/parser.h"
#include "services/pobengine.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const Item helmet = parseItem(u"Item Class: Helmets\nRarity: Rare\nStorm Crown\nTwig Circlet\n--------\n"
                              u"+80 to maximum Life\n+30% to Fire Resistance"_s)
                        .value();

// 80% fire and 60% cold, with nothing on
const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Witch\">"
                      u"<PlayerStat stat=\"FireResist\" value=\"75\"/>"
                      u"<PlayerStat stat=\"FireResistOverCap\" value=\"5\"/>"
                      u"<PlayerStat stat=\"ColdResist\" value=\"60\"/>"
                      u"</Build></PathOfBuilding2>"_s;

// The resistances of the character you play.
struct Resists
{
    QTemporaryDir dir;
    CharacterStore store{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    Resistances resists{&active};

    QString play(const Character &character, const QString &xml = {})
    {
        const QString id = store.add(character).value_or(QString());
        if (!xml.isEmpty())
            store.savePobBuild(id, xml);
        active.setId(id);
        return id;
    }

    const std::optional<ResistRecord> &saved() const { return store.find(active.id())->resists; }
};

Character tester(const Equipped &gear = {})
{
    Character c;
    c.name = u"Tester"_s;
    c.gear = gear;
    return c;
}

// As importing the build took them.
Character withTheBuilds()
{
    Character c = tester();
    PerElement<std::optional<double>> values;
    values[Element::Fire] = 80;
    values[Element::Cold] = 60;
    c.resists = snapshotResists({.values = values, .cap = {}, .buffer = {}}, {}, ResistSource::Pob);
    return c;
}

} // namespace

class TestResistances : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void nobody()
    {
        Resists r;
        QVERIFY(!r.resists.isKnown());
        QVERIFY(!r.resists.isFromSheet());
        QCOMPARE(r.resists.summary(), QString());
        QCOMPARE(r.resists.sourceNote(),
                 u"Not known yet. Import a build, or type them in from your character sheet."_s);
        QCOMPARE(r.resists.fire(), 0.0);
        QCOMPARE(r.resists.fireMaximum(), 75.0);
        QCOMPARE(r.resists.margin(), 0.0);

        r.resists.setFromSheet(true);
        QVERIFY(!r.resists.isFromSheet());
        // as nothing makes a character in this test
        QCOMPARE(r.resists.error(), u"There's no character. Add one first."_s);
    }

    void notKnownYet()
    {
        Resists r;
        r.play(tester());
        QVERIFY(!r.resists.isKnown());
        QVERIFY(!r.resists.isFromSheet());
        QCOMPARE(r.resists.summary(), QString());

        // there's nothing to set them on
        r.resists.setFireMaximum(80);
        r.resists.setMargin(3);
        QVERIFY(!r.saved());
    }

    void typedIn()
    {
        Resists r;
        r.play(tester({{u"Helmet"_s, helmet}}));
        QSignalSpy changed(&r.resists, &Resistances::changed);
        r.resists.setFire(70);
        QVERIFY(changed.size() > 0);
        QVERIFY(r.resists.isKnown());
        QVERIFY(r.resists.isFromSheet());
        QCOMPARE(r.resists.fire(), 70.0);
        QCOMPARE(r.resists.cold(), 0.0);
        QCOMPARE(r.resists.summary(), u"Fire 70%, Cold 0%, Lightning 0%, Chaos 0%"_s);
        QCOMPARE(r.resists.sourceNote(), u"Typed in from your character sheet, with what you wear now."_s);
        // read off the sheet, so it needs less to spare
        QCOMPARE(r.resists.margin(), 5.0);

        const ResistRecord saved = r.saved().value();
        QCOMPARE(saved.source, ResistSource::Game);
        QCOMPARE(saved.entered[Element::Fire], 70.0);
        QCOMPARE(saved.entered[Element::Chaos], 0.0);
        QCOMPARE(saved.gear[Element::Fire], 30.0);

        // the rest stay as they were
        r.resists.setCold(55);
        r.resists.setLightning(-10);
        r.resists.setChaos(-30);
        QCOMPARE(r.resists.summary(), u"Fire 70%, Cold 55%, Lightning -10%, Chaos -30%"_s);
    }

    void sameValueIsNoChange()
    {
        Resists r;
        r.play(tester());
        r.resists.setFire(70);
        QSignalSpy changed(&r.resists, &Resistances::changed);
        r.resists.setFire(70);
        r.resists.setFire(qQNaN());
        QCOMPARE(changed.size(), 0);
    }

    void followsTheGear()
    {
        Resists r;
        r.play(tester({{u"Helmet"_s, helmet}}));
        r.resists.setFire(70);
        QVERIFY(r.active.takeOff(u"Helmet"_s));
        QCOMPARE(r.resists.fire(), 40.0);
        QVERIFY(r.resists.isFromSheet());
        QVERIFY(r.active.wear(u"Helmet"_s, helmet));
        QCOMPARE(r.resists.fire(), 70.0);
    }

    void fromTheSheetAndBack()
    {
        Resists r;
        r.play(withTheBuilds(), build);
        QVERIFY(r.resists.isKnown());
        QVERIFY(!r.resists.isFromSheet());
        QCOMPARE(r.resists.summary(), u"Fire 80%, Cold 60%"_s);
        QCOMPARE(r.resists.sourceNote(), u"From your build in Path of Building, with what you wear now."_s);
        QCOMPARE(r.resists.lightning(), 0.0);
        QCOMPARE(r.resists.margin(), 15.0);

        // which starts from the build's
        r.resists.setFromSheet(true);
        QVERIFY(r.resists.isFromSheet());
        QCOMPARE(r.resists.summary(), u"Fire 80%, Cold 60%, Lightning 0%, Chaos 0%"_s);
        QCOMPARE(r.resists.margin(), 5.0);
        r.resists.setFire(90);

        r.resists.setFromSheet(false);
        QVERIFY(!r.resists.isFromSheet());
        QCOMPARE(r.resists.summary(), u"Fire 80%, Cold 60%"_s);
        QCOMPARE(r.saved()->source, ResistSource::Pob);
        QCOMPARE(r.resists.error(), QString());
    }

    void typingOneInTypesThemAll()
    {
        Resists r;
        r.play(withTheBuilds(), build);
        r.resists.setFire(80);
        QVERIFY(r.resists.isFromSheet());
        QCOMPARE(r.resists.summary(), u"Fire 80%, Cold 60%, Lightning 0%, Chaos 0%"_s);
    }

    void alreadyThatWay()
    {
        Resists r;
        r.play(withTheBuilds(), build);
        QSignalSpy changed(&r.resists, &Resistances::changed);
        r.resists.setFromSheet(false);
        QCOMPARE(changed.size(), 0);

        r.resists.setFromSheet(true);
        const auto typedIn = changed.size();
        QVERIFY(typedIn > 0);
        r.resists.setFromSheet(true);
        QCOMPARE(changed.size(), typedIn);
    }

    void buildWithoutResistances()
    {
        Resists r;
        r.play(tester(), build);
        r.resists.setFromSheet(false);
        QVERIFY(!r.resists.isKnown());
        QVERIFY(!r.saved());
    }

    void alreadyPlaying()
    {
        Resists r;
        r.play(withTheBuilds());
        Resistances later(&r.active);
        QVERIFY(later.isKnown());
        QCOMPARE(later.summary(), u"Fire 80%, Cold 60%"_s);
    }

    void backToNone()
    {
        Resists r;
        r.play(tester());
        r.resists.setFire(70);
        r.resists.setFromSheet(false);
        QVERIFY(!r.resists.isKnown());
        QVERIFY(!r.saved());
    }

    void maximumAndMargin()
    {
        Resists r;
        r.play(withTheBuilds(), build);
        r.resists.setFireMaximum(78);
        r.resists.setMargin(12);
        QCOMPARE(r.resists.fireMaximum(), 78.0);
        QCOMPARE(r.resists.margin(), 12.0);
        QCOMPARE(r.saved()->cap[Element::Fire], 78.0);
        QVERIFY(r.saved()->capSet[Element::Fire]);
        QVERIFY(r.saved()->bufferSet);

        // which typing them in keeps, and going back to the build's
        r.resists.setFromSheet(true);
        QCOMPARE(r.resists.fireMaximum(), 78.0);
        QCOMPARE(r.resists.margin(), 12.0);
        r.resists.setFromSheet(false);
        QCOMPARE(r.resists.fireMaximum(), 78.0);
        QCOMPARE(r.resists.margin(), 12.0);

        // none at all is fine, a maximum of none isn't
        r.resists.setMargin(0);
        QCOMPARE(r.resists.margin(), 0.0);
        r.resists.setFireMaximum(0);
        r.resists.setMargin(-1);
        QCOMPARE(r.resists.fireMaximum(), 78.0);
        QCOMPARE(r.resists.margin(), 0.0);

        // and numbers only
        r.resists.setFireMaximum(qQNaN());
        r.resists.setFireMaximum(qInf());
        r.resists.setMargin(qQNaN());
        r.resists.setMargin(qInf());
        QCOMPARE(r.resists.fireMaximum(), 78.0);
        QCOMPARE(r.resists.margin(), 0.0);
    }

    void eachMaximumOnItsOwn()
    {
        Resists r;
        r.play(withTheBuilds(), build);
        r.resists.setColdMaximum(80);
        r.resists.setChaosMaximum(70);
        QCOMPARE(r.resists.fireMaximum(), 75.0);
        QCOMPARE(r.resists.coldMaximum(), 80.0);
        QCOMPARE(r.resists.lightningMaximum(), 75.0);
        QCOMPARE(r.resists.chaosMaximum(), 70.0);
        const ResistRecord saved = r.saved().value();
        QCOMPARE(saved.cap, (PerElement<double>{{75, 80, 75, 70}}));
        QCOMPARE(saved.capSet, (PerElement<bool>{{false, true, false, true}}));
        // and each is the one that counts for its own resistance
        QCOMPARE(liveResists(saved, {})->caps, (PerElement<double>{{75, 80, 75, 70}}));
    }

    void maximumsFromTheBuildUnlessSet()
    {
        // the build says its fire maximum is 80, with 3 of that from its gear
        const QString maxed = u"<PathOfBuilding2><Build level=\"90\" className=\"Witch\">"
                              u"<PlayerStat stat=\"FireResist\" value=\"75\"/>"
                              u"<PlayerStat stat=\"FireResistMax\" value=\"80\"/>"
                              u"<PlayerStat stat=\"ColdResist\" value=\"60\"/>"
                              u"<PlayerStat stat=\"ColdResistMax\" value=\"78\"/>"
                              u"</Build></PathOfBuilding2>"_s;
        Resists r;
        r.play(tester(), maxed);
        // takes the build's
        r.resists.setFromSheet(true);
        r.resists.setFromSheet(false);
        QCOMPARE(r.resists.fireMaximum(), 80.0);
        QCOMPARE(r.resists.coldMaximum(), 78.0);
        QCOMPARE(r.resists.lightningMaximum(), 75.0);

        // one set by hand stays, the others follow the build
        r.resists.setColdMaximum(76);
        r.resists.setFromSheet(true);
        r.resists.setFromSheet(false);
        QCOMPARE(r.resists.fireMaximum(), 80.0);
        QCOMPARE(r.resists.coldMaximum(), 76.0);
    }

    void sameMaximumAndMargin()
    {
        Resists r;
        r.play(withTheBuilds(), build);
        r.resists.setFireMaximum(78);
        r.resists.setMargin(12);
        QSignalSpy changed(&r.resists, &Resistances::changed);
        r.resists.setFireMaximum(78);
        r.resists.setMargin(12);
        QCOMPARE(changed.size(), 0);
    }

    void cantSave()
    {
        Resists r;
        const QString id = r.play(tester());
        // where a folder is in the way
        const QString file = r.dir.filePath(u"characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QSignalSpy errorChanged(&r.resists, &Resistances::errorChanged);
        r.resists.setFire(70);
        QVERIFY(r.resists.error().startsWith(u"Couldn't save Tester: "_s));
        QCOMPARE(errorChanged.size(), 1);
        QVERIFY(!r.resists.isKnown());

        // and once it can
        QVERIFY(QDir().rmdir(file));
        r.resists.setFire(70);
        QCOMPARE(r.resists.error(), QString());
        QCOMPARE(errorChanged.size(), 2);
        QVERIFY(r.resists.isKnown());
        r.resists.setFire(75);
        QCOMPARE(errorChanged.size(), 2);
    }

    void anotherCharacter()
    {
        Resists r;
        r.play(withTheBuilds());
        QVERIFY(r.resists.isKnown());
        QSignalSpy changed(&r.resists, &Resistances::changed);
        r.play(tester());
        QVERIFY(changed.size() > 0);
        QVERIFY(!r.resists.isKnown());
    }
};

QTEST_GUILESS_MAIN(TestResistances)
#include "tst_resistances.moc"
