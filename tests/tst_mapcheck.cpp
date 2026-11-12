// SPDX-License-Identifier: MIT
#include "core/mapcheck.h"
#include "core/parser.h"
#include "maptexts.h"

#include <QTest>

using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

MapCheck check(const QString &text, const MapBuild &build = {})
{
    const Item i = item(text);
    return checkMap(i, mapKindOf(i).value(), build);
}

// Resistances with the same value in each element, under the cap of 75.
MapBuild withResists(double fire, double chaos = -20)
{
    LiveResists live;
    live.values[Element::Fire] = fire;
    live.values[Element::Cold] = fire;
    live.values[Element::Lightning] = fire;
    live.values[Element::Chaos] = chaos;
    for (const Element e : allElements)
        live.caps[e] = 75;
    MapBuild build;
    build.imported = true;
    build.resists = live;
    return build;
}

} // namespace

class TestMapCheck : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void kinds()
    {
        QCOMPARE(mapKindOf(item(maptexts::good)), MapKind::Waystone);
        QCOMPARE(mapKindOf(item(maptexts::tablet)), MapKind::Tablet);
        QVERIFY(!mapKindOf(item(u"Item Class: Rings\nRarity: Rare\nA\nGold Ring"_s)));
    }

    void modsAreGroupedWithTheirRewards()
    {
        const MapCheck c = check(maptexts::good);
        QCOMPARE(c.mods.size(), 2);
        QCOMPARE(c.mods.at(0).lines.size(), 3);
        QCOMPARE(c.mods.at(0).gives, (QStringList{u"7% more pack size"_s, u"10% more Waystones found"_s}));
        QCOMPARE(c.mods.at(1).lines.first(), u"Monsters are Evasive"_s);
    }

    void aGoodOne()
    {
        const MapCheck c = check(maptexts::good);
        QCOMPARE(c.grade, MapGrade::Good);
        QCOMPARE(c.badMods(), 0);
        QCOMPARE(c.mods.at(0).danger, Danger::Mild);
        QVERIFY(c.suggestions.first().startsWith(u"Run it as it is."_s));
        QVERIFY(!c.usedBuild);
    }

    void oneRiskyModIsFine()
    {
        const MapCheck c = check(maptexts::fine);
        QCOMPARE(c.grade, MapGrade::Fine);
        QCOMPARE(c.badMods(), 1);
        QCOMPARE(c.mods.at(2).danger, Danger::Risky);
        QVERIFY(c.mods.at(2).isBad());
        // a bad one in four, whichever it takes off
        QVERIFY(c.suggestions.first().contains(u"1 in 4"_s));
        QVERIFY(c.suggestions.last().startsWith(u"Or run it as it is"_s));
    }

    void aModThatEndsTheRunIsBad()
    {
        const MapCheck c = check(maptexts::bad);
        QCOMPARE(c.grade, MapGrade::Bad);
        QCOMPARE(c.mods.at(2).danger, Danger::Deadly);
        QCOMPARE(c.mods.at(2).costs.left(18), u"You recover much l"_s);
        // extra fire damage at 12 is risky
        QCOMPARE(c.mods.at(3).danger, Danger::Risky);
        QCOMPARE(c.badMods(), 2);
        QVERIFY(c.suggestions.first().contains(u"2 in 4"_s));
        QVERIFY(c.suggestions.last().startsWith(u"Don't run it"_s));
    }

    void twoRiskyModsAreBad()
    {
        const QString text =
            maptexts::fine + u"Monsters fire 2 additional Projectiles\n9% more Pack size\n"_s;
        QCOMPARE(check(text).grade, MapGrade::Bad);
    }

    void dangerGrowsWithTheRoll_data()
    {
        QTest::addColumn<QString>("mod");
        QTest::addColumn<int>("danger");
        QTest::newRow("fire low") << u"Monsters deal 5% of Damage as Extra Fire"_s << int(Danger::Mild);
        QTest::newRow("fire mid") << u"Monsters deal 12% of Damage as Extra Cold"_s << int(Danger::Risky);
        QTest::newRow("fire high") << u"Monsters deal 22% of Damage as Extra Lightning"_s
                                   << int(Danger::Deadly);
        QTest::newRow("chaos") << u"Monsters deal 15% of Damage as Extra Chaos"_s << int(Danger::Deadly);
        QTest::newRow("recovery low")
            << u"Players have 25% less Recovery Rate of Life and Energy Shield"_s << int(Danger::Risky);
        QTest::newRow("recovery high")
            << u"Players have 40% less Recovery Rate of Life and Energy Shield"_s << int(Danger::Deadly);
        QTest::newRow("max res small") << u"-3% maximum Player Resistances"_s << int(Danger::Mild);
        QTest::newRow("max res 5") << u"-5% maximum Player Resistances"_s << int(Danger::Mild);
        QTest::newRow("max res 6") << u"-6% maximum Player Resistances"_s << int(Danger::Risky);
        QTest::newRow("max res 8") << u"-8% maximum Player Resistances"_s << int(Danger::Risky);
        QTest::newRow("max res 9") << u"-9% maximum Player Resistances"_s << int(Danger::Deadly);
        QTest::newRow("max res 10") << u"-10(-10--8)% maximum Player Resistances"_s << int(Danger::Deadly);
        QTest::newRow("no damage")
            << u"Players and their Minions deal no damage for 3 out of every 10 seconds"_s
            << int(Danger::Deadly);
        QTest::newRow("curse") << u"Players are periodically Cursed with Enfeeble"_s << int(Danger::Risky);
        QTest::newRow("armoured") << u"Monsters are Armoured"_s << int(Danger::Mild);
        QTest::newRow("life low") << u"12% more Monster Life"_s << int(Danger::Mild);
        QTest::newRow("life high") << u"28% more Monster Life"_s << int(Danger::Risky);
        QTest::newRow("penetration") << u"Monster Damage Penetrates 15% Elemental Resistances"_s
                                     << int(Danger::Deadly);
    }

    void dangerGrowsWithTheRoll()
    {
        QFETCH(QString, mod);
        QFETCH(int, danger);
        const MapCheck c = check(maptexts::header + mod + u"\n10% more Waystones found in Area"_s);
        QCOMPARE(c.mods.size(), 1);
        QCOMPARE(int(c.mods.first().danger), danger);
        QVERIFY(!c.mods.first().costs.isEmpty());
    }

    void rollRangesAreIgnored()
    {
        const MapCheck c = check(maptexts::advanced);
        QCOMPARE(c.mods.at(0).lines.first(), u"Monsters deal 12% of Damage as Extra Fire"_s);
        QCOMPARE(c.mods.at(2).danger, Danger::Deadly);
    }

    void unknownMods()
    {
        const MapCheck c = check(maptexts::header + u"Monsters sing a song\n"_s);
        QCOMPARE(c.mods.size(), 1);
        QVERIFY(!c.mods.first().known);
        QCOMPARE(c.mods.first().danger, Danger::Harmless);
        QCOMPARE(c.grade, MapGrade::Good);
    }

    void nothingOnIt()
    {
        const MapCheck c = check(u"Item Class: Waystones\nRarity: Normal\nWaystone (Tier 1)\n"
                                 u"--------\nWaystone Tier: 1\n--------\nItem Level: 65"_s);
        QVERIFY(c.mods.isEmpty());
        QCOMPARE(c.grade, MapGrade::Fine);
        QVERIFY(c.suggestions.isEmpty());
    }

    void sidesCameFromTheHeaders()
    {
        const MapCheck c = check(maptexts::advanced);
        QCOMPARE(c.mods.at(0).side, AffixSide::Prefix);
        QCOMPARE(c.mods.at(1).side, AffixSide::Suffix);
        QCOMPARE(c.mods.at(2).side, AffixSide::Suffix);
        // two bad of four: 1 in 2 of the prefixes, 1 in 2 of the suffixes, no better than 2 in 4
        QCOMPARE(c.badMods(), 2);
        QVERIFY(c.suggestions.first().contains(u"2 in 4"_s));
        QCOMPARE(c.suggestions.size(), 3);
    }

    void anOmenImprovesTheOdds()
    {
        // one bad prefix of one prefix, three suffixes
        const QString text =
            maptexts::header +
            u"{ Prefix Modifier \"Heated\" (Tier: 1) }\n"
            u"Monsters deal 22% of Damage as Extra Fire\n14% more Rarity of Items found in this Area\n"
            u"{ Suffix Modifier \"of Ease\" (Tier: 1) }\nMonsters are Evasive\n6% more Pack size\n"
            u"{ Suffix Modifier \"of Ease\" (Tier: 1) }\nMonsters are Armoured\n"
            u"{ Suffix Modifier \"of Ease\" (Tier: 1) }\nMonsters have 12% increased Accuracy Rating"_s;
        const MapCheck c = check(text);
        QVERIFY(c.suggestions.first().contains(u"1 in 4"_s));
        QVERIFY(c.suggestions.at(1).contains(u"Omen of Sinistral Annulment"_s));
        QVERIFY(c.suggestions.at(1).contains(u"1 in 1"_s));
        QVERIFY(!c.suggestions.join(u' ').contains(u"Dextral"_s));
    }

    void plainCopiesHaveNoOmens()
    {
        QVERIFY(!check(maptexts::bad).suggestions.join(u' ').contains(u"Omen"_s));
    }

    void everyModBad()
    {
        const MapCheck c = check(maptexts::header + u"-9% maximum Player Resistances\n"_s);
        QVERIFY(c.suggestions.first().contains(u"whichever it picks"_s));
    }

    void corruptedCantBeChanged()
    {
        const MapCheck c = check(maptexts::bad + u"\n--------\nCorrupted"_s);
        QVERIFY(c.corrupted);
        QCOMPARE(c.grade, MapGrade::Bad);
        QCOMPARE(c.suggestions.size(), 1);
        QVERIFY(c.suggestions.first().startsWith(u"It's corrupted"_s));
        QCOMPARE(c.mods.size(), 4);
    }

    void tablets()
    {
        const MapCheck c = check(maptexts::tablet);
        QCOMPARE(c.kind, MapKind::Tablet);
        QCOMPARE(c.mods.size(), 2);
        QCOMPARE(c.mods.at(0).gives, (QStringList{u"30% more magic and rare monsters"_s}));
        QCOMPARE(c.mods.at(0).danger, Danger::Harmless);
        QCOMPARE(c.mods.at(1).danger, Danger::Risky);
        QCOMPARE(c.grade, MapGrade::Fine);
    }

    void yourBuildMakesModsWorse()
    {
        const QString text =
            maptexts::header +
            u"Monsters deal 12% of Damage as Extra Fire\n10% more Waystones found in Area\n"_s;
        const MapCheck plain = check(text);
        QCOMPARE(plain.mods.first().danger, Danger::Risky);
        QVERIFY(!plain.usedBuild);

        // capped, so nothing changes
        const MapCheck capped = check(text, withResists(80));
        QVERIFY(capped.usedBuild);
        QVERIFY(!capped.buildChanged);
        QCOMPARE(capped.mods.first().danger, Danger::Risky);

        // under the cap, it's worse
        const MapCheck under = check(text, withResists(40));
        QVERIFY(under.buildChanged);
        QCOMPARE(under.mods.first().danger, Danger::Deadly);
        QCOMPARE(under.mods.first().baseDanger, Danger::Risky);
        QVERIFY(under.mods.first().buildNote.contains(u"fire resistance is 40%"_s));
        QCOMPARE(under.grade, MapGrade::Bad);
    }

    void onlyTheElementItDealsCounts()
    {
        const QString text = maptexts::header + u"Monsters deal 12% of Damage as Extra Fire\n"_s;
        MapBuild build = withResists(80);
        build.resists->values[Element::Cold] = 10;
        QVERIFY(!check(text, build).buildChanged);
    }

    void chaosAndPenetration()
    {
        const MapCheck chaos =
            check(maptexts::header + u"Monsters deal 7% of Damage as Extra Chaos\n"_s, withResists(80, -30));
        QCOMPARE(chaos.mods.first().baseDanger, Danger::Mild);
        QCOMPARE(chaos.mods.first().danger, Danger::Risky);
        QVERIFY(chaos.mods.first().buildNote.contains(u"chaos"_s));

        const MapCheck pen = check(
            maptexts::header + u"Monster Damage Penetrates 7% Elemental Resistances\n"_s, withResists(60));
        QCOMPARE(pen.mods.first().danger, Danger::Deadly);
        QCOMPARE(pen.mods.first().buildNote,
                 u"Your fire, cold and lightning resistances are under the cap."_s);

        // one or two under the cap read as a sentence too
        MapBuild two = withResists(80);
        two.resists->values[Element::Cold] = 40;
        const QString penetrates =
            maptexts::header + u"Monster Damage Penetrates 7% Elemental Resistances\n"_s;
        QCOMPARE(check(penetrates, two).mods.first().buildNote, u"Your cold resistance is under the cap."_s);
        two.resists->values[Element::Fire] = 40;
        QCOMPARE(check(penetrates, two).mods.first().buildNote,
                 u"Your fire and cold resistances are under the cap."_s);
    }

    void aThinHealthPool()
    {
        const QString text =
            maptexts::header + u"12% increased Monster Damage\n10% more Waystones found in Area\n"_s;
        MapBuild build;
        build.imported = true;
        build.health = 2400;
        const MapCheck thin = check(text, build);
        QVERIFY(thin.buildChanged);
        QCOMPARE(thin.mods.first().danger, Danger::Risky);
        QVERIFY(thin.mods.first().buildNote.contains(u"2,400"_s));

        build.health = 5000;
        const MapCheck fat = check(text, build);
        QVERIFY(fat.usedBuild);
        QVERIFY(!fat.buildChanged);
        QCOMPARE(fat.mods.first().danger, Danger::Mild);
    }

    void modsWithTwoStatsStayTogether()
    {
        const MapCheck c = check(maptexts::pairs);
        QCOMPARE(c.mods.size(), 2);
        QCOMPARE(c.mods.at(0).lines.size(), 4);
        QCOMPARE(c.mods.at(1).lines.size(), 4);
        QVERIFY(c.mods.at(1).costs.contains(u"Monster crits hit harder"_s));
        QVERIFY(c.mods.at(0).known && c.mods.at(1).known);
    }

    void desecratedMods()
    {
        const MapCheck c = check(maptexts::desecrated);
        QCOMPARE(c.mods.size(), 5);
        for (const MapMod &m : c.mods)
            QVERIFY(m.known);
        QCOMPARE(c.mods.at(0).danger, Danger::Harmless);
        QCOMPARE(c.mods.at(1).danger, Danger::Mild);
        QCOMPARE(c.mods.at(2).danger, Danger::Deadly);
        QCOMPARE(c.mods.at(3).lines.size(), 2);
        QCOMPARE(c.mods.at(3).danger, Danger::Risky);
        QCOMPARE(c.mods.at(4).lines.size(), 2);
        QCOMPARE(c.mods.at(4).danger, Danger::Mild);
        QCOMPARE(c.grade, MapGrade::Bad);
    }

    void pageRangesAreRolledAway()
    {
        const MapCheck c =
            check(maptexts::good + u"\nMonsters deal (10\u201414)% of Damage as Extra Cold\n"_s);
        QCOMPARE(c.mods.last().lines.first(), u"Monsters deal % of Damage as Extra Cold"_s);
    }

    void areaLevelFromTheTier()
    {
        QCOMPARE(areaLevelOf(item(maptexts::good)), 79);
        QCOMPARE(areaLevelOf(item(u"Item Class: Waystones\nRarity: Rare\nA\nWaystone (Tier 1)\n"
                                  u"--------\nWaystone Tier: 1\n--------\nItem Level: 65\n"_s)),
                 65);
        QCOMPARE(areaLevelOf(item(u"Item Class: Waystones\nRarity: Rare\nA\nWaystone\n--------\n"
                                  u"Area Level: 72\n--------\nItem Level: 72\n"_s)),
                 72);
        QCOMPARE(check(maptexts::good).areaLevel, 79);
    }

    void resistancePenaltyByAreaLevel()
    {
        QCOMPARE(resistPenaltyAt(0), 0);
        QCOMPARE(resistPenaltyAt(15), 0);
        QCOMPARE(resistPenaltyAt(16), 10);
        QCOMPARE(resistPenaltyAt(32), 10);
        QCOMPARE(resistPenaltyAt(33), 20);
        QCOMPARE(resistPenaltyAt(45), 30);
        QCOMPARE(resistPenaltyAt(54), 40);
        QCOMPARE(resistPenaltyAt(60), 50);
        QCOMPARE(resistPenaltyAt(64), 50);
        QCOMPARE(resistPenaltyAt(65), 60);
        QCOMPARE(resistPenaltyAt(80), 60);
        QCOMPARE(check(maptexts::good).resistPenalty, 60);
    }

    void thePenaltyIsInTheResistanceNote()
    {
        const MapCheck c = check(maptexts::bad, withResists(70));
        const QString note = c.mods.at(3).buildNote;
        QVERIFY(note.contains(u"under the cap"_s));
        QVERIFY(note.contains(u"takes 60% off"_s));
        QVERIFY(note.contains(u"leaving 10%"_s));
        // an item that doesn't say its level has no penalty to count
        const MapCheck none =
            check(u"Item Class: Waystones\nRarity: Rare\nA\nWaystone\n--------\n"
                  u"Item Level: 79\n--------\nMonsters deal 12% of Damage as Extra Fire\n"
                  u"Monsters have 16% more Effectiveness\n15% more Waystones found in Area"_s,
                  withResists(70));
        QVERIFY(!none.mods.first().buildNote.contains(u"area level"_s));
    }

    void odds()
    {
        QCOMPARE(oddsText(1, 3), u"1 in 3"_s);
        QCOMPARE(dangerName(Danger::Deadly), u"Can end the run"_s);
    }
};

QTEST_APPLESS_MAIN(TestMapCheck)
#include "tst_mapcheck.moc"
