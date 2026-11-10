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
        QCOMPARE(labelsOf(boots), (QStringList{u"Movement speed"_s, u"Life"_s, u"Fire res"_s}));
        // the same minimums a craft base search asks for
        QCOMPARE(boots.at(0).minimum, searchMinimum(35));
        QCOMPARE(boots.at(1).minimum, searchMinimum(100));
        QCOMPARE(wantedText(boots), u"Movement speed 19 or more (up to 35), Life 55 or more (up to 149), "
                                    u"Fire res 24 or more (up to 45)"_s);
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
                 u"Fire res 24 or more (up to 45), Cold res 24 or more (up to 45), "
                 u"Lightning res 24 or more (up to 45)"_s);

        // exactly the minimum is enough
        const Item exact = item(u"Boots"_s, u"+55 to maximum Life\n19% increased Movement Speed"_s);
        QCOMPARE(labelsOf(wantedStats(u"Boots"_s, &exact, {})),
                 (QStringList{u"Fire res"_s, u"Cold res"_s, u"Lightning res"_s}));

        // under the minimum is still wanted
        const Item weak = item(u"Boots"_s, u"+40 to maximum Life\n12% increased Movement Speed"_s);
        QCOMPARE(labelsOf(wantedStats(u"Boots"_s, &weak, {})),
                 (QStringList{u"Movement speed"_s, u"Life"_s, u"Fire res"_s}));

        const Item rolled = item(u"Boots"_s, lifeAndSpeed + u"\n"_s + threeResists);
        QVERIFY(wantedStats(u"Boots"_s, &rolled, {}).isEmpty());
    }

    void yourTweaksDecideWhatCounts()
    {
        const QList<WantedStat> noLife = wantedStats(u"Boots"_s, nullptr, {.tweaks = {{u"Life"_s, 0}}});
        QVERIFY(!labelsOf(noLife).contains(u"Life"_s));
        // a stat worth under 4 isn't searched for
        const QList<WantedStat> minor =
            wantedStats(u"Boots"_s, nullptr, {.tweaks = {{u"Movement speed"_s, 0.2}}});
        QVERIFY(!labelsOf(minor).contains(u"Movement speed"_s));
        // and a stat made to count more comes first
        const QList<WantedStat> first =
            wantedStats(u"Boots"_s, nullptr, {.tweaks = {{u"Lightning res"_s, 3}}});
        QCOMPARE(first.first().label, u"Lightning res"_s);
    }

    void ignoredStatsAreNotWanted()
    {
        const GearScoreOptions options{
            .ignore = [](const QString &key) { return key.contains(u"Movement Speed"); }};
        QVERIFY(!labelsOf(wantedStats(u"Boots"_s, nullptr, options)).contains(u"Movement speed"_s));
    }

    void resistancesAreWantedUntilCapped()
    {
        GearScoreOptions options;
        options.resistNeed[Element::Fire] = 10.2;
        options.resistNeed[Element::Cold] = 0;
        options.resistNeed[Element::Lightning] = 80;
        const QList<WantedStat> wants = wantedStats(u"Helmet"_s, nullptr, options, 5);
        // cold is capped, fire needs 11 at most, and lightning the usual
        QCOMPARE(wantedText(wants), u"Life 55 or more (up to 174), Fire res 11 or more (up to 45), "
                                    u"Lightning res 24 or more (up to 45)"_s);
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
