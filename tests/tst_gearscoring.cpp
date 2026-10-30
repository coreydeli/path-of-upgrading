// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/gearscoring.h"
#include "core/buildprofiles.h"
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

// How the gear of the character you play is scored, as the window shows it.
struct Scoring
{
    QTemporaryDir dir;
    CharacterStore store{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    GearScoring scoring{&active};
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

constexpr int asUsual = 2;

} // namespace

class TestGearScoring : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nobody()
    {
        Scoring s;
        QCOMPARE(s.scoring.rowCount(), 0);
        QCOMPARE(s.scoring.character(), QString());
        QVERIFY(!s.scoring.isTweaked());
        QCOMPARE(s.scoring.profiles(),
                 QStringList({u"Life and resistances"_s, u"Energy Shield spellcaster"_s,
                              u"Minion summoner on Energy Shield"_s, u"None, by my stat weights alone"_s}));
        QCOMPARE(s.scoring.levels().size(), 6);
        QCOMPARE(s.scoring.levels().at(asUsual), u"As usual"_s);
    }

    void defaultProfile()
    {
        Scoring s;
        QSignalSpy changed(&s.scoring, &GearScoring::changed);
        s.play(tester());
        QCOMPARE(s.scoring.character(), u"Tester"_s);
        QCOMPARE(s.scoring.profile(), 0);
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
        GearScoring later(&s.active);
        QCOMPARE(later.character(), u"Tester"_s);
        QCOMPARE(later.rowCount(), s.scoring.rowCount());
        QVERIFY(later.rowCount() > 0);
        QCOMPARE(later.index(0).data(GearScoring::LabelRole),
                 s.scoring.index(0).data(GearScoring::LabelRole));
    }

    void pickProfile()
    {
        Scoring s;
        s.play(tester());
        QSignalSpy reset(&s.scoring, &GearScoring::modelReset);
        s.scoring.setProfile(1);
        QCOMPARE(s.saved().buildProfile, u"es-caster"_s);
        QCOMPARE(s.scoring.profile(), 1);
        QCOMPARE(s.labels(), labelsOf(u"es-caster"));
        QCOMPARE(s.scoring.summary(), buildProfile(u"es-caster")->summary);
        QCOMPARE(reset.size(), 1);

        // and back to the first, saved by its id
        s.scoring.setProfile(0);
        QCOMPARE(s.saved().buildProfile, u"life-res"_s);
        QCOMPARE(s.labels(), labelsOf(defaultBuildProfile));
        QCOMPARE(reset.size(), 2);
    }

    void noProfile()
    {
        Scoring s;
        s.play(tester());
        const int none = int(s.scoring.profiles().size()) - 1;
        s.scoring.setProfile(none);
        QCOMPARE(s.saved().buildProfile, noBuildProfile.toString());
        QCOMPARE(s.scoring.profile(), none);
        QCOMPARE(s.scoring.rowCount(), 0);
        QCOMPARE(s.scoring.summary(), u"Items are judged by your stat weights alone, with no gear score."_s);

        // with no rows to change
        QSignalSpy changed(&s.scoring, &GearScoring::changed);
        s.active.reload();
        QCOMPARE(changed.size(), 1);
    }

    void unknownProfile()
    {
        // as for none, saved by a newer version maybe
        Scoring s;
        s.play(tester(u"glass-cannon"_s));
        QCOMPARE(s.scoring.profile(), int(s.scoring.profiles().size()) - 1);
        QCOMPARE(s.scoring.rowCount(), 0);
    }

    void outOfRange()
    {
        Scoring s;
        s.play(tester());
        QSignalSpy changed(&s.active, &ActiveCharacter::changed);
        s.scoring.setProfile(-1);
        s.scoring.setProfile(int(s.scoring.profiles().size()));
        s.scoring.setProfile(0);
        s.scoring.setLevel(u"Life"_s, -1);
        s.scoring.setLevel(u"Life"_s, int(s.scoring.levels().size()));
        s.scoring.setLevel(u"Life"_s, asUsual);
        QCOMPARE(changed.size(), 0);
        QCOMPARE(s.saved().buildProfile, QString());
    }

    void where()
    {
        Scoring s;
        s.play(tester(u"es-caster"_s));
        QCOMPARE(s.of(u"Movement speed"_s, GearScoring::WhereRole).toString(), u"Counts on Boots"_s);
        QCOMPARE(s.of(u"Spell skill levels"_s, GearScoring::WhereRole).toString(),
                 u"Counts on Weapon, Offhand, and Amulet"_s);
        QCOMPARE(s.of(u"Life"_s, GearScoring::WhereRole).toString(),
                 u"Counts in every slot but Weapon and Jewel"_s);
        QCOMPARE(s.of(u"Mana"_s, GearScoring::WhereRole).toString(), u"Counts in every slot but Jewel"_s);
        // half of them
        QCOMPARE(s.of(u"Rarity"_s, GearScoring::WhereRole).toString(),
                 u"Counts on Helmet, Gloves, Boots, Amulet, and Ring"_s);

        s.scoring.setProfile(2);
        QCOMPARE(s.of(u"Intelligence"_s, GearScoring::WhereRole).toString(), u"Counts in every slot"_s);
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
                    .endsWith(u". Not at all to start with, as your build has Chaos Inoculation"_s));
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
        QCOMPARE(s.scoring.rowCount(), 0);
        QCOMPARE(reset.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestGearScoring)
#include "tst_gearscoring.moc"
