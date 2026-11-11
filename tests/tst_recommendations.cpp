// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/recommendations.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

Item item(const QString &itemClass, const QString &mods, const QString &rarity = u"Rare"_s)
{
    return parseItem(u"Item Class: %1\nRarity: %2\nDusk Mark\nIron Base\n--------\n%3"_s.arg(itemClass,
                                                                                             rarity, mods))
        .value();
}

const QString lifeAndSpeed = u"+60 to maximum Life\n20% increased Movement Speed"_s;
const QString threeResists =
    u"+30% to Fire Resistance\n+30% to Cold Resistance\n+30% to Lightning Resistance"_s;

QStringList labelsOf(const QList<WantedStat> &stats)
{
    QStringList labels;
    for (const WantedStat &s : stats)
        labels << s.label;
    return labels;
}

QStringList slotsOf(const QList<Recommendation> &list)
{
    QStringList slots;
    for (const Recommendation &r : list)
        slots << r.slot;
    return slots;
}

} // namespace

class TestRecommendations : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void anEmptySlotWantsItsTopStats()
    {
        const QList<WantedStat> boots = wantedStats(u"Boots"_s, nullptr, {});
        QCOMPARE(labelsOf(boots), (QStringList{u"Movement Speed"_s, u"Life"_s, u"Fire Resistance"_s}));
        // the same minimums a craft base search asks for
        QCOMPARE(boots.at(0).minimum, searchMinimum(35));
        QCOMPARE(boots.at(1).minimum, searchMinimum(100));
        QCOMPARE(wantedText(boots), u"Movement Speed 19 or more (up to 35), Life 55 or more (up to 149), "
                                    u"Fire Resistance 24 or more (up to 45)"_s);
        QCOMPARE(wantedStats(u"Boots"_s, nullptr, {}, 5).size(), 5);
        QCOMPARE(wantedStats(u"Boots"_s, nullptr, {}, 1).size(), 1);
    }

    void aProfileWithNoSearchableStats()
    {
        // weapons are rated by damage per second, which can't be searched for
        QVERIFY(wantedStats(u"Weapon"_s, nullptr, {}).isEmpty());
        QVERIFY(wantedStats(u"Boots"_s, nullptr, {.build = u"nothing like it"_s}).isEmpty());
        QCOMPARE(wantedText({}), QString());
    }

    void whatTheWornItemHasIsLeftOut()
    {
        const Item boots = item(u"Boots"_s, lifeAndSpeed);
        QCOMPARE(wantedText(wantedStats(u"Boots"_s, &boots, {})),
                 u"Fire Resistance 24 or more (up to 45), Cold Resistance 24 or more (up to 45), "
                 u"Lightning Resistance 24 or more (up to 45)"_s);

        // exactly the minimum is enough
        const Item exact = item(u"Boots"_s, u"+55 to maximum Life\n19% increased Movement Speed"_s);
        QCOMPARE(labelsOf(wantedStats(u"Boots"_s, &exact, {})),
                 (QStringList{u"Fire Resistance"_s, u"Cold Resistance"_s, u"Lightning Resistance"_s}));

        // under the minimum is still wanted
        const Item weak = item(u"Boots"_s, u"+40 to maximum Life\n12% increased Movement Speed"_s);
        QCOMPARE(labelsOf(wantedStats(u"Boots"_s, &weak, {})),
                 (QStringList{u"Movement Speed"_s, u"Life"_s, u"Fire Resistance"_s}));

        const Item rolled = item(u"Boots"_s, lifeAndSpeed + u"\n"_s + threeResists);
        QVERIFY(wantedStats(u"Boots"_s, &rolled, {}).isEmpty());
    }

    void whatASlotCanRollIsItsTopTiers()
    {
        const QList<RollRange> boots = canRoll(u"Boots"_s, nullptr, {});
        QCOMPARE(boots.size(), 5);
        QCOMPARE(boots.first().label, u"Movement Speed"_s);
        const auto top = SlotMods::builtIn().best(u"Boots"_s, u"#% increased Movement Speed"_s);
        QVERIFY(top);
        QCOMPARE(boots.first().top, *top);
        QCOMPARE(rollRangeText(boots.first()),
                 u"Movement Speed %1, item level %2"_s.arg(rollText(top->high)).arg(top->level));
        QCOMPARE(canRoll(u"Boots"_s, nullptr, {}, 2).size(), 2);

        // what's worn doesn't matter, only its class
        const Item worn = item(u"Boots"_s, lifeAndSpeed + u"\n"_s + threeResists);
        QCOMPARE(canRoll(u"Boots"_s, &worn, {}), boots);
        const Item gloves = item(u"Gloves"_s, lifeAndSpeed);
        QVERIFY(canRoll(u"Boots"_s, &gloves, {}) != boots);

        // your tweaks and ignored stats decide which stats are wanted
        const auto life = [](const QList<RollRange> &list) {
            for (const RollRange &r : list) {
                if (r.label == u"Life"_s)
                    return true;
            }
            return false;
        };
        QVERIFY(life(boots));
        QVERIFY(!life(canRoll(u"Boots"_s, nullptr, {.tweaks = {{u"Life"_s, 0}}})));
    }

    void aRangeWithOneEndIsOneNumber()
    {
        QCOMPARE(rollRangeText({.label = u"Life"_s, .top = {.level = 5, .low = 20, .high = 20}}),
                 u"Life 20, item level 5"_s);
        QCOMPARE(rollRangeText({.label = u"Life"_s, .top = {.level = 82, .low = 10.5, .high = 12}}),
                 u"Life 10.5 to 12, item level 82"_s);
    }

    void nothingToRollWithoutAClassOrATable()
    {
        QVERIFY(canRoll(u"Weapon"_s, nullptr, {}).isEmpty());
        QVERIFY(canRoll(u"Offhand"_s, nullptr, {}).isEmpty());
        QVERIFY(canRoll(u"Jewel"_s, nullptr, {}).isEmpty());
        QVERIFY(canRoll(u"Boots"_s, nullptr, {.build = u"nothing like it"_s}).isEmpty());
        const SlotMods none;
        QVERIFY(canRoll(u"Boots"_s, nullptr, {.mods = &none}).isEmpty());
    }

    void yourTweaksDecideWhatCounts()
    {
        const QList<WantedStat> noLife = wantedStats(u"Boots"_s, nullptr, {.tweaks = {{u"Life"_s, 0}}});
        QVERIFY(!labelsOf(noLife).contains(u"Life"_s));
        // a stat worth under 4 isn't searched for
        const QList<WantedStat> minor =
            wantedStats(u"Boots"_s, nullptr, {.tweaks = {{u"Movement Speed"_s, 0.2}}});
        QVERIFY(!labelsOf(minor).contains(u"Movement Speed"_s));
        // and a stat made to count more comes first
        const QList<WantedStat> first =
            wantedStats(u"Boots"_s, nullptr, {.tweaks = {{u"Lightning Resistance"_s, 3}}});
        QCOMPARE(first.first().label, u"Lightning Resistance"_s);
    }

    void ignoredStatsAreNotWanted()
    {
        const GearScoreOptions options{
            .ignore = [](const QString &key) { return key.contains(u"Movement Speed"); }};
        QVERIFY(!labelsOf(wantedStats(u"Boots"_s, nullptr, options)).contains(u"Movement Speed"_s));
    }

    void resistancesAreWantedUntilCapped()
    {
        GearScoreOptions options;
        options.resistNeed[Element::Fire] = 10.2;
        options.resistNeed[Element::Cold] = 0;
        options.resistNeed[Element::Lightning] = 80;
        const QList<WantedStat> wants = wantedStats(u"Helmet"_s, nullptr, options, 5);
        // cold is capped, fire needs 11 at most, and lightning the usual
        QCOMPARE(wantedText(wants), u"Life 55 or more (up to 174), Fire Resistance 11 or more (up to 45), "
                                    u"Lightning Resistance 24 or more (up to 45)"_s);
    }

    void thePickedSlotsGoByTheirScores()
    {
        Character c;
        c.gear[u"Helmet"_s] = item(u"Helmets"_s, lifeAndSpeed + u"\n"_s + threeResists);
        c.gear[u"Body Armour"_s] = item(u"Body Armours"_s, u"+10 to maximum Life"_s);
        c.gear[u"Gloves"_s] = item(u"Gloves"_s, u"+40 to maximum Life\n+20% to Fire Resistance"_s);
        c.gear[u"Boots"_s] = item(u"Boots"_s, u"+40 to maximum Life\n+20% to Fire Resistance"_s);
        c.gear[u"Belt"_s] = item(u"Belts"_s, u"+25 to maximum Life"_s);
        c.gear[u"Amulet"_s] = item(u"Amulets"_s, u"+25 to maximum Life"_s);
        c.gear[u"Ring 1"_s] = item(u"Rings"_s, u"+25 to maximum Life"_s);
        c.gear[u"Ring 2"_s] = item(u"Rings"_s, u"+25 to maximum Life"_s);
        c.gear[u"Weapon"_s] = item(u"One Hand Swords"_s, u"+25 to maximum Life"_s);

        // the empty offhand has the build's stats to look for, the helmet
        // has nothing left to look for, and the rest go by their scores
        const QList<Recommendation> picked = recommendedUpgrades(c, {}, 10);
        QVERIFY(!slotsOf(picked).contains(u"Helmet"_s));
        QCOMPARE(picked.first().slot, u"Offhand"_s);
        QVERIFY(!picked.first().score);
        QVERIFY(!picked.first().wants.isEmpty());
        QCOMPARE(picked.size(), 8);
        for (qsizetype i = 2; i < picked.size(); ++i)
            QVERIFY2(*picked.at(i - 1).score <= *picked.at(i).score, qPrintable(picked.at(i).slot));

        // three at most, unless it's asked for more
        QCOMPARE(recommendedUpgrades(c).size(), 3);
        QCOMPARE(slotsOf(recommendedUpgrades(c)), slotsOf(picked.first(3)));
        QCOMPARE(recommendedUpgrades(c, {}, 0).size(), 0);

        const Recommendation &worst = picked.at(1);
        QVERIFY(worst.score);
        QVERIFY(!worst.grade.isEmpty());
        QCOMPARE(wantedText(worst.wants).isEmpty(), false);
    }

    void tiesKeepTheSlotOrder()
    {
        Character c;
        c.gear[u"Ring 2"_s] = item(u"Rings"_s, u"+25 to maximum Life"_s);
        c.gear[u"Ring 1"_s] = item(u"Rings"_s, u"+25 to maximum Life"_s);
        const QStringList slots = slotsOf(recommendedUpgrades(c, {}, 20));
        QVERIFY(slots.indexOf(u"Ring 1"_s) < slots.indexOf(u"Ring 2"_s));
    }

    void emptySlotsComeBeforeWornOnes()
    {
        Character c;
        c.gear[u"Helmet"_s] = item(u"Helmets"_s, u"+25 to maximum Life"_s);
        const QStringList slots = slotsOf(recommendedUpgrades(c, {}, 20));
        QVERIFY(slots.indexOf(u"Helmet"_s) > slots.indexOf(u"Boots"_s));
        QCOMPARE(slots.last(), u"Helmet"_s);
    }

    void uniquesHaveNoScoreToRank()
    {
        Character c;
        c.gear[u"Helmet"_s] = item(u"Helmets"_s, u"+25 to maximum Life"_s, u"Unique"_s);
        QVERIFY(!slotsOf(recommendedUpgrades(c, {}, 20)).contains(u"Helmet"_s));
    }

    void noOffhandWithATwoHander()
    {
        Character c;
        QVERIFY(slotsOf(recommendedUpgrades(c, {}, 20)).contains(u"Offhand"_s));
        c.gear[u"Weapon"_s] = item(u"Two Hand Swords"_s, u"+25 to maximum Life"_s);
        QVERIFY(c.gear.value(u"Weapon"_s).twoHanded);
        QVERIFY(!slotsOf(recommendedUpgrades(c, {}, 20)).contains(u"Offhand"_s));
    }

    void offhandCountsWhenTheCharacterCanUseBoth()
    {
        Character c;
        c.gear[u"Weapon"_s] = item(u"Two Hand Swords"_s, u"+25 to maximum Life"_s);
        c.wielding.twoHandedInOne = true;
        QVERIFY(slotsOf(recommendedUpgrades(c, {}, 20)).contains(u"Offhand"_s));
        // a staff isn't one Giant's Blood covers
        c.gear[u"Weapon"_s] = item(u"Staves"_s, u"+25 to maximum Life"_s);
        QVERIFY(!slotsOf(recommendedUpgrades(c, {}, 20)).contains(u"Offhand"_s));
        c.wielding = {.focusWithStaff = true};
        QVERIFY(slotsOf(recommendedUpgrades(c, {}, 20)).contains(u"Offhand"_s));
    }

    void aPlayerWithNothingWornStillHasSomethingToLookFor()
    {
        const QList<Recommendation> picked = recommendedUpgrades(Character{});
        QCOMPARE(picked.size(), 3);
        for (const Recommendation &r : picked)
            QVERIFY(!r.wants.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestRecommendations)
#include "tst_recommendations.moc"
