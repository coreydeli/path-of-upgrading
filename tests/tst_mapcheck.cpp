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
        QTest::newRow("max res") << u"-6% maximum Player Resistances"_s << int(Danger::Deadly);
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
        const MapCheck c = check(maptexts::header + u"-6% maximum Player Resistances\n"_s);
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
        QVERIFY(pen.mods.first().buildNote.contains(u"fire, cold, lightning"_s));
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

    void odds()
    {
        QCOMPARE(oddsText(1, 3), u"1 in 3"_s);
        QCOMPARE(dangerName(Danger::Deadly), u"Can end the run"_s);
    }
};

QTEST_APPLESS_MAIN(TestMapCheck)
#include "tst_mapcheck.moc"
