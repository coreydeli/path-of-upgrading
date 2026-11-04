// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/gearlist.h"
#include "app/settingsstore.h"
#include "core/evaluation.h"
#include "core/parser.h"
#include "core/resists.h"
#include "core/slots.h"
#include "services/pobengine.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Item item(const QString &itemClass, const QString &rarity, const QString &name, const QString &base,
          const QString &mods)
{
    return parseItem(
               u"Item Class: %1\nRarity: %2\n%3\n%4\n--------\n%5"_s.arg(itemClass, rarity, name, base, mods))
        .value();
}

const Item helmet = item(u"Helmets"_s, u"Rare"_s, u"Storm Crown"_s, u"Twig Circlet"_s,
                         u"+80 to maximum Life\n+30% to Fire Resistance"_s);
const Item ring = item(u"Rings"_s, u"Rare"_s, u"Dusk Loop"_s, u"Iron Ring"_s, u"+30% to Cold Resistance"_s);
const Item belt =
    item(u"Belts"_s, u"Unique"_s, u"Coil of Ash"_s, u"Rawhide Belt"_s, u"+40 to maximum Life"_s);
const Item jewel = item(u"Jewels"_s, u"Rare"_s, u"Pale Eye"_s, u"Ruby"_s, u"+8% to Fire Resistance"_s);

// The gear of the character you play, as the window lists it.
struct Gear
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    CharacterStore store{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    GearList list{&active, &settings};
    QAbstractItemModelTester tester{&list, QAbstractItemModelTester::FailureReportingMode::QtTest};

    QString play(const Character &character)
    {
        const QString id = store.add(character).value_or(QString());
        active.setId(id);
        return id;
    }

    QStringList slotNames() const
    {
        QStringList names;
        for (int row = 0; row < list.rowCount(); ++row)
            names << list.index(row).data(GearList::SlotRole).toString();
        return names;
    }

    QVariant in(const QString &slot, GearList::Role role) const
    {
        const auto row = slotNames().indexOf(slot);
        return row < 0 ? QVariant() : list.index(int(row)).data(role);
    }
};

Character wearing(std::initializer_list<std::pair<QString, Item>> gear)
{
    Character c;
    c.name = u"Tester"_s;
    for (const auto &[slot, item] : gear)
        c.gear.insert(slot, item);
    return c;
}

} // namespace

class TestGearList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void nobody()
    {
        Gear g;
        // with the main slots to fill, all empty
        QCOMPARE(g.list.character(), QString());
        QCOMPARE(g.slotNames(), pou::mainSlots());
        QCOMPARE(g.in(u"Helmet"_s, GearList::NameRole).toString(), QString());
        QVERIFY(!g.list.hasExtras());
    }

    void mainSlots()
    {
        Gear g;
        Character c = wearing({{u"Helmet"_s, helmet}, {u"Ring 2"_s, ring}});
        c.fromPob = {u"Helmet"_s};
        QSignalSpy changed(&g.list, &GearList::changed);
        g.play(c);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(g.list.character(), u"Tester"_s);
        QVERIFY(!g.list.hasExtras());
        QCOMPARE(g.slotNames(), pou::mainSlots());

        QCOMPARE(g.in(u"Helmet"_s, GearList::NameRole).toString(), u"Storm Crown"_s);
        QCOMPARE(g.in(u"Helmet"_s, GearList::BaseRole).toString(), u"Twig Circlet"_s);
        QVERIFY(g.in(u"Helmet"_s, GearList::FromPobRole).toBool());
        QVERIFY(!g.in(u"Helmet"_s, GearList::ExtraRole).toBool());
        const std::optional<GearScore> score = gearScore(helmet, gearScoreOptions(c, u"Helmet"_s));
        QVERIFY(score && score->isRated());
        QCOMPARE(g.in(u"Helmet"_s, GearList::ScoreRole).toString(), QString::number(score->score));
        QCOMPARE(g.in(u"Helmet"_s, GearList::GradeRole).toString(), score->grade);
        QCOMPARE(g.list.index(2).data(Qt::DisplayRole).toString(), u"Helmet"_s);

        QCOMPARE(g.in(u"Ring 2"_s, GearList::NameRole).toString(), u"Dusk Loop"_s);
        QVERIFY(!g.in(u"Ring 2"_s, GearList::FromPobRole).toBool());

        QCOMPARE(g.in(u"Gloves"_s, GearList::NameRole).toString(), QString());
        QCOMPARE(g.in(u"Gloves"_s, GearList::BaseRole).toString(), QString());
        QCOMPARE(g.in(u"Gloves"_s, GearList::ScoreRole).toString(), QString());
        QCOMPARE(g.in(u"Gloves"_s, GearList::GradeRole).toString(), QString());
    }

    void extras()
    {
        Gear g;
        g.play(wearing({{u"Jewel 3"_s, jewel}}));
        QVERIFY(g.list.hasExtras());
        QCOMPARE(g.slotNames(), pou::mainSlots() + QStringList{u"Jewel 3"_s});
        QVERIFY(g.in(u"Jewel 3"_s, GearList::ExtraRole).toBool());
        QCOMPARE(g.in(u"Jewel 3"_s, GearList::NameRole).toString(), u"Pale Eye"_s);
    }

    void unique()
    {
        Gear g;
        g.play(wearing({{u"Belt"_s, belt}}));
        QCOMPARE(g.in(u"Belt"_s, GearList::NameRole).toString(), u"Coil of Ash"_s);
        QCOMPARE(g.in(u"Belt"_s, GearList::ScoreRole).toString(), QString());
        QCOMPARE(g.in(u"Belt"_s, GearList::GradeRole).toString(), QString());
    }

    void overall()
    {
        // over the main slots, the weakest first
        Gear g;
        const Item gloves =
            item(u"Gloves"_s, u"Rare"_s, u"Grim Grip"_s, u"Suede Bracers"_s, u"+10 to maximum Life"_s);
        const Item boots =
            item(u"Boots"_s, u"Rare"_s, u"Ash Tread"_s, u"Rawhide Boots"_s, u"+50 to maximum Life"_s);
        const Character c = wearing({{u"Helmet"_s, helmet},
                                     {u"Ring 1"_s, ring},
                                     {u"Gloves"_s, gloves},
                                     {u"Boots"_s, boots},
                                     {u"Belt"_s, belt},
                                     {u"Jewel 1"_s, jewel}});
        g.play(c);
        QMap<int, QString> bySlot;
        QList<int> main;
        for (const QString &slot : {u"Helmet"_s, u"Ring 1"_s, u"Gloves"_s, u"Boots"_s}) {
            const int score = gearScore(c.gear.value(slot), gearScoreOptions(c, slot))->score;
            bySlot.insert(score, slot);
            main << score;
        }
        // they're all different, so the order is clear
        QCOMPARE(bySlot.size(), 4);
        QCOMPARE(bySlot.first(), u"Gloves"_s);

        const double average = std::round((main[0] + main[1] + main[2] + main[3]) / 4.0);
        QCOMPARE(g.list.overall(), QString::number(average));
        QCOMPARE(g.list.overallGrade(), gearGrade(int(average)));
        QStringList weakest;
        for (auto it = bySlot.cbegin(); weakest.size() < 3; ++it)
            weakest << it.value() + u' ' + QString::number(it.key());
        QCOMPARE(g.list.weakest(), weakest.join(u", "_s));
    }

    // the notes of a slot, as the pairs the window shows
    static QList<QStringList> notesOf(const QVariant &notes)
    {
        QList<QStringList> out;
        for (const GearNote &note : notes.value<QList<GearNote>>())
            out << QStringList{note.label, note.text};
        return out;
    }

    void whatToImprove()
    {
        Gear g;
        const Character c = wearing({{u"Helmet"_s, helmet}, {u"Belt"_s, belt}, {u"Jewel 3"_s, jewel}});
        g.play(c);

        // what the popup would say of the helmet, a note to each
        const GearScore score = *gearScore(helmet, gearScoreOptions(c, u"Helmet"_s));
        QVERIFY(!score.missing.isEmpty());
        const QList<QStringList> notes = notesOf(g.in(u"Helmet"_s, GearList::NotesRole));
        QCOMPARE(notes.first(), (QStringList{u"Missing"_s, score.missing.join(u", "_s)}));
        QCOMPARE(notes.size(), int(!score.missing.isEmpty()) + int(!score.notScored.isEmpty()) +
                                   int(!weakRolls(score).isEmpty()));

        // a unique has no gear score, so nothing to improve
        QCOMPARE(notesOf(g.in(u"Belt"_s, GearList::NotesRole)), QList<QStringList>());
        QVERIFY(!notesOf(g.in(u"Jewel 3"_s, GearList::NotesRole)).isEmpty());
    }

    void wastedAndWeak()
    {
        // a rarity mod is no use to the build, and a tenth of a roll is weak
        Gear g;
        const Item armour = item(u"Body Armours"_s, u"Rare"_s, u"Dusk Mantle"_s, u"Plated Vest"_s,
                                 u"+10 to maximum Life\n10% increased Rarity of Items found"_s);
        const Character c = wearing({{u"Body Armour"_s, armour}});
        g.play(c);
        const GearScore score = *gearScore(armour, gearScoreOptions(c, u"Body Armour"_s));
        QVERIFY(!score.notScored.isEmpty());
        QVERIFY(!weakRolls(score).isEmpty());
        QCOMPARE(notesOf(g.in(u"Body Armour"_s, GearList::NotesRole)),
                 (QList<QStringList>{
                     {u"Missing"_s, score.missing.join(u", "_s)},
                     {u"Not scored"_s, notScoredStats(score).join(u", "_s)},
                     {u"Weak rolls"_s, weakRolls(score).join(u", "_s) + u" of an excellent roll"_s}}));
    }

    void whatTheBuildWantsInAnEmptySlot()
    {
        Gear g;
        const Character c = wearing({{u"Helmet"_s, helmet}});
        g.play(c);
        const QList<KeyStat> top = topStats(u"Gloves"_s, 3, gearScoreOptions(c, u"Gloves"_s));
        QVERIFY(!top.isEmpty());
        QStringList labels;
        for (const KeyStat &stat : top)
            labels << stat.label;
        QCOMPARE(notesOf(g.in(u"Gloves"_s, GearList::NotesRole)),
                 (QList<QStringList>{{u"Wants most"_s, labels.join(u", "_s)}}));

        // before there's a character too, and not by your weights alone
        Gear nobody;
        QCOMPARE(notesOf(nobody.in(u"Gloves"_s, GearList::NotesRole)).first().first(), u"Wants most"_s);
        Character weights = wearing({});
        weights.buildProfile = noBuildProfile.toString();
        nobody.play(weights);
        QCOMPARE(notesOf(nobody.in(u"Gloves"_s, GearList::NotesRole)), QList<QStringList>());
        QCOMPARE(notesOf(nobody.in(u"Helmet"_s, GearList::NotesRole)), QList<QStringList>());
    }

    void focus()
    {
        // on the weakest slot, which the overall test has as the gloves
        Gear g;
        const Item gloves =
            item(u"Gloves"_s, u"Rare"_s, u"Grim Grip"_s, u"Suede Bracers"_s, u"+10 to maximum Life"_s);
        const Character c = wearing({{u"Helmet"_s, helmet}, {u"Gloves"_s, gloves}});
        g.play(c);
        QCOMPARE(g.list.weakest().section(u' ', 0, 0), u"Gloves"_s);
        // only the first thing to do, the rest is in the slot's notes
        const GearScore score = *gearScore(gloves, gearScoreOptions(c, u"Gloves"_s));
        QCOMPARE(g.list.focus(), u"Start with Gloves. "_s + missingNote(score));
        QVERIFY(notesOf(g.in(u"Gloves"_s, GearList::NotesRole)).size() > 1);

        QSignalSpy changed(&g.list, &GearList::changed);
        QVERIFY(g.active.takeOff(u"Gloves"_s));
        QVERIFY(changed.size() > 0);
        QVERIFY(g.list.focus().startsWith(u"Start with Helmet. "_s));

        // and with nothing to rate, nothing to say
        QVERIFY(g.active.takeOff(u"Helmet"_s));
        QCOMPARE(g.list.focus(), QString());
    }

    void noOverall()
    {
        Gear g;
        g.play(wearing({{u"Belt"_s, belt}, {u"Jewel 1"_s, jewel}}));
        QCOMPARE(g.list.overall(), QString());
        QCOMPARE(g.list.overallGrade(), QString());
        QCOMPARE(g.list.weakest(), QString());

        // nor by your weights alone
        Character c = wearing({{u"Helmet"_s, helmet}});
        c.buildProfile = noBuildProfile.toString();
        g.play(c);
        QCOMPARE(g.list.overall(), QString());
        QCOMPARE(g.list.weakest(), QString());
    }

    void sameName()
    {
        // a magic item, whose name is its base
        Gear g;
        Item magic = ring;
        magic.name = magic.baseType;
        g.play(wearing({{u"Ring 1"_s, magic}}));
        QCOMPARE(g.in(u"Ring 1"_s, GearList::NameRole).toString(), u"Iron Ring"_s);
        QCOMPARE(g.in(u"Ring 1"_s, GearList::BaseRole).toString(), QString());
    }

    void followsWhatYouWear()
    {
        Gear g;
        g.play(wearing({}));
        QSignalSpy changed(&g.list, &GearList::changed);
        QVERIFY(g.active.wear(u"Jewel 1"_s, jewel));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(g.in(u"Jewel 1"_s, GearList::NameRole).toString(), u"Pale Eye"_s);
    }

    void takeOff()
    {
        Gear g;
        const QString id = g.play(wearing({{u"Helmet"_s, helmet}, {u"Jewel 2"_s, jewel}}));
        QSignalSpy errorChanged(&g.list, &GearList::errorChanged);
        QVERIFY(g.list.takeOff(u"Helmet"_s));
        QCOMPARE(g.in(u"Helmet"_s, GearList::NameRole).toString(), QString());
        QCOMPARE(g.store.find(id)->gear.keys(), QStringList{u"Jewel 2"_s});

        QVERIFY(g.list.takeOff(u"Jewel 2"_s));
        QCOMPARE(g.slotNames(), pou::mainSlots());
        QVERIFY(!g.list.hasExtras());
        QCOMPARE(g.list.error(), QString());
        QCOMPARE(errorChanged.size(), 0);
    }

    void cantTakeOff()
    {
        Gear g;
        const QString id = g.play(wearing({{u"Helmet"_s, helmet}}));
        // where a folder is in the way
        const QString file = g.dir.filePath(u"characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QSignalSpy errorChanged(&g.list, &GearList::errorChanged);
        QVERIFY(!g.list.takeOff(u"Helmet"_s));
        QVERIFY(g.list.error().startsWith(u"Couldn't save Tester: "_s));
        QCOMPARE(errorChanged.size(), 1);
        QCOMPARE(g.in(u"Helmet"_s, GearList::NameRole).toString(), u"Storm Crown"_s);

        // and once it can
        QVERIFY(QDir().rmdir(file));
        QVERIFY(g.list.takeOff(u"Helmet"_s));
        QCOMPARE(g.list.error(), QString());
        QCOMPARE(errorChanged.size(), 2);
    }

    void followsPriorities()
    {
        // as resistances can count for more
        Gear g;
        g.play(wearing({{u"Helmet"_s, helmet}}));
        QSignalSpy reset(&g.list, &GearList::modelReset);
        Settings s = g.settings.settings();
        s.overlay.hotkey = u"F3"_s;
        g.settings.set(s);
        QCOMPARE(reset.size(), 0);

        s.pob.priorities.resistsFirst = !s.pob.priorities.resistsFirst;
        g.settings.set(s);
        QCOMPARE(reset.size(), 1);

        // and not again for something else
        s.overlay.hotkey = u"F4"_s;
        g.settings.set(s);
        QCOMPARE(reset.size(), 1);
    }

    void scoresWithYourPriorities()
    {
        // short of fire resistance only
        Character c = wearing({{u"Helmet"_s, helmet}});
        ResistEntry entry;
        for (const Element e : allElements)
            entry.values[e] = 75.0;
        entry.values[Element::Fire] = 30.0;
        c.resists = snapshotResists(entry, c.gear, ResistSource::Manual);

        PobPriorities first;
        first.resistsFirst = true;
        PobPriorities notFirst;
        notFirst.resistsFirst = false;
        const auto scoreWith = [&](const PobPriorities &priorities) {
            return QString::number(gearScore(helmet, gearScoreOptions(c, u"Helmet"_s, priorities))->score);
        };
        QCOMPARE_NE(scoreWith(first), scoreWith(notFirst));

        Gear g;
        g.play(c);
        Settings s = g.settings.settings();
        s.pob.priorities = first;
        g.settings.set(s);
        QCOMPARE(g.in(u"Helmet"_s, GearList::ScoreRole).toString(), scoreWith(first));
        s.pob.priorities = notFirst;
        g.settings.set(s);
        QCOMPARE(g.in(u"Helmet"_s, GearList::ScoreRole).toString(), scoreWith(notFirst));

        // and the ones it started with
        Settings started = g.settings.settings();
        started.pob.priorities = first;
        g.settings.set(started);
        GearList later(&g.active, &g.settings);
        const int row = int(g.slotNames().indexOf(u"Helmet"_s));
        QCOMPARE(later.index(row).data(GearList::ScoreRole).toString(), scoreWith(first));
    }
};

QTEST_GUILESS_MAIN(TestGearList)
#include "tst_gearlist.moc"
