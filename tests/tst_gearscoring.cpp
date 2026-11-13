// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/buildweigher.h"
#include "app/gearscoring.h"
#include "core/buildprofiles.h"
#include "core/weighedprofile.h"
#include "services/pobengine.h"

#include <QAbstractItemModelTester>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// How the gear of the character you play is scored, as the window shows it.
struct Scoring
{
    QTemporaryDir dir;
    CharacterStore store{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    BuildWeigher weigher{&active, &engine};
    GearScoring scoring{&active, &weigher};
    QAbstractItemModelTester tester{&scoring, QAbstractItemModelTester::FailureReportingMode::QtTest};

    QString play(const Character &character)
    {
        const QString id = store.add(character).value_or(QString());
        active.setId(id);
        return id;
    }

    QStringList labels() const
    {
        QStringList names;
        for (int row = 0; row < scoring.rowCount(); ++row)
            names << scoring.index(row).data(GearScoring::LabelRole).toString();
        return names;
    }

    QVariant of(const QString &label, GearScoring::Role role) const
    {
        const auto row = labels().indexOf(label);
        return row < 0 ? QVariant() : scoring.index(int(row)).data(role);
    }

    const Character &saved() const { return *store.find(active.id()); }
};

Character tester(const QString &profile = {})
{
    Character c;
    c.name = u"Tester"_s;
    c.buildProfile = profile;
    return c;
}

QStringList labelsOf(QStringView profile)
{
    QStringList labels;
    for (const TweakableStat &s : tweakableStats(*buildProfile(profile)))
        labels << s.label;
    return labels;
}

// Path of Building's weighing of boots, for the build on this date.
WeighedProfile weighedBoots()
{
    WeighedProfile weighed = weighedProfile(
        {{u"Boots"_s, {{u"#% increased Movement Speed"_s, 1, 30}, {u"# to maximum Life"_s, 0.2, 100}}}},
        *buildProfile(defaultBuildProfile));
    weighed.pobVersion = u"0.3.3"_s;
    weighed.date = u"2026-03-14"_s;
    return weighed;
}

constexpr int asUsual = 2;

} // namespace

class TestGearScoring : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nobody()
    {
        // which has the default profile's stats to change
        Scoring s;
        QCOMPARE(s.scoring.rowCount(), int(tweakableStats(*buildProfile(defaultBuildProfile)).size()));
        QCOMPARE(s.scoring.character(), QString());
        QVERIFY(!s.scoring.isTweaked());
        QCOMPARE(s.scoring.levels().size(), 6);
        QCOMPARE(s.scoring.levels(),
                 QStringList({u"Not at all (0x)"_s, u"Half (0.5x)"_s, u"Normal (1x)"_s,
                              u"One and a half (1.5x)"_s, u"Double (2x)"_s, u"Three times (3x)"_s}));
    }

    void defaultProfile()
    {
        Scoring s;
        QSignalSpy changed(&s.scoring, &GearScoring::changed);
        s.play(tester());
        QCOMPARE(s.scoring.character(), u"Tester"_s);
        QCOMPARE(s.scoring.summary(), buildProfile(defaultBuildProfile)->summary);
        QCOMPARE(s.labels(), labelsOf(defaultBuildProfile));
        QCOMPARE(s.of(u"Life"_s, GearScoring::LevelRole).toInt(), asUsual);
        QVERIFY(!s.scoring.isTweaked());
        QCOMPARE(changed.size(), 1);
    }

    void alreadyPlaying()
    {
        Scoring s;
        s.play(tester(u"es-caster"_s));
        GearScoring later(&s.active, &s.weigher);
        QCOMPARE(later.character(), u"Tester"_s);
        QCOMPARE(later.rowCount(), s.scoring.rowCount());
        QVERIFY(later.rowCount() > 0);
        QCOMPARE(later.index(0).data(GearScoring::LabelRole),
                 s.scoring.index(0).data(GearScoring::LabelRole));
    }

    void unknownProfile()
    {
        // saved by a newer version maybe, so nothing to rate by
        Scoring s;
        s.play(tester(u"glass-cannon"_s));
        QCOMPARE(s.scoring.rowCount(), 0);
    }

    void aGeneralStart()
    {
        Scoring s;
        s.play(tester());
        QCOMPARE(s.scoring.state(), u"A general start, until Path of Building weighs your build."_s);
        QCOMPARE(s.scoring.weighedOn(), QString());
        QVERIFY(!s.scoring.weightsOnly());
        QVERIFY(!s.scoring.isWeighing());
        QVERIFY(!s.scoring.canWeigh());
    }

    void weighedForTheBuild()
    {
        Scoring s;
        Character c = tester();
        c.weighed = weighedBoots();
        s.play(c);
        const QString date = QLocale().toString(QDate(2026, 3, 14), QLocale::LongFormat);
        QCOMPARE(s.scoring.weighedOn(), date);
        QCOMPARE(s.scoring.state(), u"Weighed by Path of Building for your build on "_s + date + u"."_s);
        QVERIFY(s.scoring.summary().startsWith(u"Slots Path of Building didn't weigh go by the "_s));

        // the weighed stats are the rows, with the start's for the other slots
        QCOMPARE(s.of(u"Movement Speed"_s, GearScoring::WhereRole).toString(), u"Counts on Boots."_s);
        QVERIFY(s.labels().contains(u"Rarity"_s));
    }

    void weighingChangesTheRows()
    {
        // the start profile counts life on boots, and the weighing took over there
        Scoring s;
        const QString id = s.play(tester());
        const QStringList before = s.labels();
        QSignalSpy reset(&s.scoring, &GearScoring::modelReset);
        QSignalSpy changed(&s.scoring, &GearScoring::changed);
        Character weighed = *s.store.find(id);
        weighed.weighed = weighedBoots();
        QVERIFY(s.store.save(id, weighed));
        s.active.reload();
        QVERIFY(s.labels() != before);
        QCOMPARE(reset.size(), 1);
        QVERIFY(changed.size() >= 1);
        QVERIFY(s.of(u"Movement Speed"_s, GearScoring::LevelRole).isValid());
    }

    void weightsOnly()
    {
        Scoring s;
        Character c = tester();
        c.weighed = weighedBoots();
        s.play(c);
        QSignalSpy changed(&s.scoring, &GearScoring::changed);
        s.scoring.setWeightsOnly(true);
        QVERIFY(s.saved().weightsOnly);
        QVERIFY(s.scoring.weightsOnly());
        QCOMPARE(s.scoring.rowCount(), 0);
        QCOMPARE(s.scoring.state(), u"Judged by your stat weights alone"_s);
        QCOMPARE(s.scoring.summary(), u"Items are judged by your stat weights alone, with no gear score."_s);
        QVERIFY(changed.size() >= 1);
        // the start and the weighing are kept
        QCOMPARE(s.saved().buildProfile, QString());
        QVERIFY(s.saved().weighed);

        // once is enough
        QSignalSpy active(&s.active, &ActiveCharacter::changed);
        s.scoring.setWeightsOnly(true);
        QCOMPARE(active.size(), 0);

        s.scoring.setWeightsOnly(false);
        QVERIFY(!s.saved().weightsOnly);
        QVERIFY(s.scoring.rowCount() > 0);
    }

    void tweaksOfVanishedStatsAreKept()
    {
        Scoring s;
        Character c = tester();
        c.tweaks = {{u"Movement Speed"_s, 3}};
        c.weighed = weighedBoots();
        const QString id = s.play(c);
        // weighing it again drops the stat, and the tweak stays, unused
        Character again = *s.store.find(id);
        again.weighed = weighedProfile({{u"Boots"_s, {{u"# to maximum Life"_s, 1, 100}}}},
                                       *buildProfile(defaultBuildProfile));
        QVERIFY(s.store.save(id, again));
        s.active.reload();
        QVERIFY(!s.labels().contains(u"Movement Speed"_s));
        QCOMPARE(s.saved().tweaks, ProfileTweaks({{u"Movement Speed"_s, 3.0}}));
    }

    void outOfRange()
    {
        Scoring s;
        s.play(tester());
        QSignalSpy changed(&s.active, &ActiveCharacter::changed);
        s.scoring.setLevel(u"Life"_s, -1);
        s.scoring.setLevel(u"Life"_s, int(s.scoring.levels().size()));
        s.scoring.setLevel(u"Life"_s, asUsual);
        QCOMPARE(changed.size(), 0);
    }

    void where()
    {
        Scoring s;
        s.play(tester(u"es-caster"_s));
        QCOMPARE(s.of(u"Movement Speed"_s, GearScoring::WhereRole).toString(), u"Counts on Boots."_s);
        QCOMPARE(s.of(u"Spell skill levels"_s, GearScoring::WhereRole).toString(),
                 u"Counts on Weapon, Offhand, and Amulet."_s);
        QCOMPARE(s.of(u"Life"_s, GearScoring::WhereRole).toString(),
                 u"Counts in every slot but Weapon and Jewel."_s);
        QCOMPARE(s.of(u"Mana"_s, GearScoring::WhereRole).toString(), u"Counts in every slot but Jewel."_s);
        // half of them
        QCOMPARE(s.of(u"Rarity"_s, GearScoring::WhereRole).toString(),
                 u"Counts on Helmet, Gloves, Boots, Amulet, and Ring."_s);

        Scoring minions;
        minions.play(tester(u"minion-es"_s));
        QCOMPARE(minions.of(u"Intelligence"_s, GearScoring::WhereRole).toString(),
                 u"Counts in every slot."_s);
    }

    void theAttackersStats()
    {
        Scoring s;
        s.play(tester(u"es-attacker"_s));
        QCOMPARE(s.of(u"Weapon damage per second"_s, GearScoring::WhereRole).toString(),
                 u"Counts on Weapon."_s);
        QCOMPARE(s.of(u"Attack Speed"_s, GearScoring::WhereRole).toString(),
                 u"Counts in every slot but Helmet, Body Armour, Boots, and Belt."_s);
        QCOMPARE(s.of(u"Movement Speed"_s, GearScoring::WhereRole).toString(), u"Counts on Boots."_s);
        QVERIFY(s.of(u"Spell skill levels"_s, GearScoring::WhereRole).toString().isEmpty());
    }

    void setLevel()
    {
        Scoring s;
        s.play(tester());
        QSignalSpy reset(&s.scoring, &GearScoring::modelReset);
        QSignalSpy dataChanged(&s.scoring, &GearScoring::dataChanged);
        QSignalSpy errorChanged(&s.scoring, &GearScoring::errorChanged);
        s.scoring.setLevel(u"Life"_s, 4);
        QCOMPARE(s.saved().tweaks, ProfileTweaks({{u"Life"_s, 2.0}}));
        QCOMPARE(s.of(u"Life"_s, GearScoring::LevelRole).toInt(), 4);
        QVERIFY(s.scoring.isTweaked());
        QCOMPARE(dataChanged.size(), 1);
        QCOMPARE(dataChanged.at(0).at(2).value<QList<int>>(),
                 QList<int>({GearScoring::WhereRole, GearScoring::LevelRole}));
        QCOMPARE(reset.size(), 0);

        s.scoring.setLevel(u"Life"_s, 0);
        QCOMPARE(s.saved().tweaks, ProfileTweaks({{u"Life"_s, 0.0}}));

        // as usual isn't kept as a tweak
        s.scoring.setLevel(u"Life"_s, asUsual);
        QCOMPARE(s.saved().tweaks, ProfileTweaks());
        QVERIFY(!s.scoring.isTweaked());
        QCOMPARE(errorChanged.size(), 0);
    }

    void nearestLevel()
    {
        // for tweaks set by hand
        Scoring s;
        Character c = tester();
        c.tweaks = {{u"Life"_s, 1.2}, {u"Speed"_s, 2.6}, {u"Rarity"_s, 9}};
        s.play(c);
        QCOMPARE(s.of(u"Life"_s, GearScoring::LevelRole).toInt(), asUsual);
        QCOMPARE(s.of(u"Speed"_s, GearScoring::LevelRole).toInt(), 5);
        QCOMPARE(s.of(u"Rarity"_s, GearScoring::LevelRole).toInt(), 5);
    }

    void savedTweaksKeepTheirLevel()
    {
        // tweaks are saved as factors, whatever the levels are called
        Scoring s;
        Character c = tester(u"es-caster"_s);
        c.tweaks = {{u"Life"_s, 0}, {u"Movement Speed"_s, 0.5}, {u"Rarity"_s, 1.5}, {u"Mana"_s, 2}};
        const QString id = s.play(c);
        QCOMPARE(s.of(u"Life"_s, GearScoring::LevelRole).toInt(), 0);
        QCOMPARE(s.of(u"Movement Speed"_s, GearScoring::LevelRole).toInt(), 1);
        QCOMPARE(s.of(u"Rarity"_s, GearScoring::LevelRole).toInt(), 3);
        QCOMPARE(s.of(u"Mana"_s, GearScoring::LevelRole).toInt(), 4);

        // and picking a level saves the factor
        s.scoring.setLevel(u"Movement Speed"_s, 5);
        QCOMPARE(s.store.find(id)->tweaks.value(u"Movement Speed"_s), 3.0);
    }

    void chaosInoculation()
    {
        // where chaos res doesn't count to start with
        Scoring s;
        Character c = tester();
        c.chaosInoculation = true;
        s.play(c);
        const QString chaos = resistLabel(Element::Chaos);
        QCOMPARE(s.of(chaos, GearScoring::LevelRole).toInt(), 0);
        QVERIFY(s.of(chaos, GearScoring::WhereRole)
                    .toString()
                    .endsWith(u". Not at all to start with, as your build has Chaos Inoculation."_s));
        QVERIFY(!s.of(u"Life"_s, GearScoring::WhereRole).toString().contains(u"Chaos"_s));
        QVERIFY(!s.scoring.isTweaked());

        s.scoring.setLevel(chaos, asUsual);
        QCOMPARE(s.saved().tweaks, ProfileTweaks({{chaos, 1.0}}));
        s.scoring.setLevel(chaos, 0);
        QCOMPARE(s.saved().tweaks, ProfileTweaks());
    }

    void resetTweaks()
    {
        Scoring s;
        Character c = tester();
        // and one for a stat this profile doesn't have
        c.tweaks = {{u"Life"_s, 2}, {u"Speed"_s, 0}, {u"Spirit"_s, 3}};
        s.play(c);
        QVERIFY(s.scoring.isTweaked());
        s.scoring.reset();
        QCOMPARE(s.saved().tweaks, ProfileTweaks({{u"Spirit"_s, 3.0}}));
        QVERIFY(!s.scoring.isTweaked());
        QCOMPARE(s.of(u"Life"_s, GearScoring::LevelRole).toInt(), asUsual);

        // nothing to save
        QSignalSpy changed(&s.active, &ActiveCharacter::changed);
        s.scoring.reset();
        QCOMPARE(changed.size(), 0);
    }

    void cantSave()
    {
        Scoring s;
        const QString id = s.play(tester());
        // where a folder is in the way
        const QString file = s.dir.filePath(u"characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QSignalSpy errorChanged(&s.scoring, &GearScoring::errorChanged);
        s.scoring.setLevel(u"Life"_s, 4);
        QVERIFY(s.scoring.error().startsWith(u"Couldn't save Tester: "_s));
        QCOMPARE(errorChanged.size(), 1);
        QCOMPARE(s.of(u"Life"_s, GearScoring::LevelRole).toInt(), asUsual);

        // and once it can
        QVERIFY(QDir().rmdir(file));
        s.scoring.setLevel(u"Life"_s, 4);
        QCOMPARE(s.scoring.error(), QString());
        QCOMPARE(errorChanged.size(), 2);
    }

    void anotherCharacter()
    {
        Scoring s;
        s.play(tester(u"es-caster"_s));
        QSignalSpy reset(&s.scoring, &GearScoring::modelReset);
        Character other = tester(u"minion-es"_s);
        other.name = u"Ottoline"_s;
        s.play(other);
        QCOMPARE(s.scoring.character(), u"Ottoline"_s);
        QCOMPARE(s.labels(), labelsOf(u"minion-es"));
        QCOMPARE(reset.size(), 1);

        // and nobody
        s.active.setId(u"gone"_s);
        QCOMPARE(s.scoring.rowCount(), int(tweakableStats(*buildProfile(defaultBuildProfile)).size()));
        QCOMPARE(reset.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestGearScoring)
#include "tst_gearscoring.moc"
