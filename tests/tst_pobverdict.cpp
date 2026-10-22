// SPDX-License-Identifier: MIT
#include "core/pobverdict.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// A minion build with every resistance but chaos capped.
const PobStats base = {
    {u"FullDPS"_s, 0},          {u"MinionDPS"_s, 100000},        {u"TotalEHP"_s, 50000},
    {u"FireResist"_s, 75},      {u"FireResistTotal"_s, 78},      {u"MissingFireResist"_s, 0},
    {u"ColdResist"_s, 75},      {u"ColdResistTotal"_s, 96},      {u"MissingColdResist"_s, 0},
    {u"LightningResist"_s, 75}, {u"LightningResistTotal"_s, 83}, {u"MissingLightningResist"_s, 0},
    {u"ChaosResist"_s, 0},      {u"ChaosResistTotal"_s, 0},      {u"MissingChaosResist"_s, 75},
};

PobStats with(PobStats stats, const PobStats &changes)
{
    stats.insert(changes);
    return stats;
}

// The same build 37 points short of the lightning cap.
const PobStats shortOfLightning = with(base, {
                                                 {u"LightningResist"_s, 38},
                                                 {u"LightningResistTotal"_s, 38},
                                                 {u"MissingLightningResist"_s, 37},
                                             });

// The same build with Spirit to spare and the attributes for its gear.
const PobStats geared = with(base, {
                                       {u"SpiritUnreserved"_s, 50},
                                       {u"Str"_s, 77},
                                       {u"ReqStr"_s, 75},
                                       {u"Int"_s, 300},
                                       {u"ReqInt"_s, 160},
                                   });

PobVerdict judged(const PobStats &before, const PobStats &after, const PobPriorities &priorities = {})
{
    PobVerdictOptions options;
    options.priorities = priorities;
    return judgeWithPob(before, after, options);
}

QList<Element> capping(const PobVerdict &v)
{
    QList<Element> out;
    for (const PobResistChange &r : v.resists) {
        if (r.reachesCap)
            out << r.element;
    }
    return out;
}

} // namespace

class TestPobVerdict : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void minionDamageAndEhp()
    {
        const PobVerdict v =
            judgeWithPob(base, with(base, {{u"MinionDPS"_s, 105000}, {u"TotalEHP"_s, 51000}}));
        QCOMPARE(v.damage.stat, u"MinionDPS"_s);
        QCOMPARE(v.damage.percent.value(), 5.0);
        QCOMPARE(v.ehp->percent.value(), 2.0);
        QCOMPARE(v.score, 3.8); // 0.6 * 5 + 0.4 * 2
        QCOMPARE(v.verdict, Verdict::Upgrade);
        QVERIFY(v.breaks.isEmpty());
        QVERIFY(v.resists.isEmpty());
        QCOMPARE(v.resistRank, 0);
    }

    void smallChanges()
    {
        QCOMPARE(judgeWithPob(base, with(base, {{u"MinionDPS"_s, 102000}})).verdict, Verdict::Small);
        QCOMPARE(judgeWithPob(base, with(base, {{u"MinionDPS"_s, 100500}})).verdict, Verdict::Same);
        QCOMPARE(judgeWithPob(base, base).verdict, Verdict::Same);
        QCOMPARE(judgeWithPob(base, with(base, {{u"MinionDPS"_s, 96000}})).verdict, Verdict::Downgrade);
    }

    void fullOrMainSkillDamage()
    {
        QCOMPARE(pobDamageStat({{u"FullDPS"_s, 5000}, {u"MinionDPS"_s, 0}, {u"CombinedDPS"_s, 4000}}),
                 u"FullDPS"_s);
        QCOMPARE(pobDamageStat({{u"FullDPS"_s, 0}, {u"MinionDPS"_s, 0}, {u"CombinedDPS"_s, 4000}}),
                 u"CombinedDPS"_s);
        QCOMPARE(pobDamageStat({{u"FullDPS"_s, 0}, {u"MinionDPS"_s, 0}}), u"TotalDPS"_s);
        QCOMPARE(pobDamageStat({}), u"TotalDPS"_s);

        const PobVerdict v =
            judgeWithPob({{u"FullDPS"_s, 0}, {u"CombinedDPS"_s, 4000}, {u"TotalEHP"_s, 9000}},
                         {{u"FullDPS"_s, 0}, {u"CombinedDPS"_s, 4400}, {u"TotalEHP"_s, 9000}});
        QCOMPARE(v.damage.stat, u"CombinedDPS"_s);
        QCOMPARE(v.damage.percent.value(), 10.0);
        QCOMPARE(v.verdict, Verdict::Upgrade);
    }

    void sameDamageNumberOnBothSides()
    {
        // the item makes Path of Building add up skills, but the build is judged by its minions
        const PobVerdict v = judgeWithPob(base, with(base, {{u"FullDPS"_s, 500000}}));
        QCOMPARE(v.damage.stat, u"MinionDPS"_s);
        QCOMPARE(v.verdict, Verdict::Same);
    }

    void damageFromNothing()
    {
        // a weapon the skill can't be used with, and one it can
        const PobVerdict v = judgeWithPob({{u"TotalDPS"_s, 0}, {u"TotalEHP"_s, 9000}},
                                          {{u"TotalDPS"_s, 4000}, {u"TotalEHP"_s, 9000}});
        QCOMPARE(v.damage.stat, u"TotalDPS"_s);
        QCOMPARE(v.damage.to, 4000.0);
        QVERIFY(!v.damage.percent);
        QCOMPARE(v.score, 15.0); // counted as 25% more
        QCOMPARE(v.verdict, Verdict::Upgrade);
        QVERIFY(!v.losesDamage);
    }

    void damageWeight()
    {
        const PobStats before = {{u"FullDPS"_s, 1000}, {u"TotalEHP"_s, 1000}};
        const PobStats after = {{u"FullDPS"_s, 1100}, {u"TotalEHP"_s, 900}};
        PobVerdictOptions options;
        options.priorities.keepDamage = false;
        options.damageWeight = 1;
        QCOMPARE(judgeWithPob(before, after, options).score, 10.0);
        options.damageWeight = 0;
        QCOMPARE(judgeWithPob(before, after, options).score, -10.0);
        options.damageWeight = 3;
        QCOMPARE(judgeWithPob(before, after, options).score, 10.0);
    }

    void unknownEhp()
    {
        const PobVerdict v = judgeWithPob({{u"FullDPS"_s, 1000}}, {{u"FullDPS"_s, 1100}});
        QVERIFY(!v.ehp);
        QCOMPARE(v.score, 6.0);
    }

    void resistanceOffTheCap()
    {
        const PobStats after = with(shortOfLightning, {{u"MinionDPS"_s, 110000}});
        const PobVerdict v = judgeWithPob(base, after);
        // its own verdict whatever the damage, and last when sorting
        QCOMPARE(v.verdict, Verdict::ResistLoss);
        QCOMPARE(v.resistRank, -1);
        QCOMPARE(v.resistGain, -37);
        QVERIFY(v.score < 0); // losing 37 points outweighs 10% more damage
        QCOMPARE(v.breaks, QStringList{u"Lightning res drops below the cap (38% of 75%)"_s});

        QCOMPARE(v.resists.size(), 1);
        const PobResistChange &r = v.resists.first();
        QCOMPARE(r.element, Element::Lightning);
        QCOMPARE(r.from, 75.0);
        QCOMPARE(r.to, 38.0);
        QCOMPARE(r.totalFrom.value(), 83.0);
        QCOMPARE(r.totalTo.value(), 38.0);
        QVERIFY(r.wasCapped);
        QVERIFY(!r.capped);
        QVERIFY(r.lost);

        PobPriorities damageFirst;
        damageFirst.resistsFirst = false;
        QCOMPARE(judged(base, after, damageFirst).verdict, Verdict::ResistLoss);
    }

    void furtherUnderTheCap()
    {
        const PobVerdict v =
            judgeWithPob(shortOfLightning, with(shortOfLightning, {
                                                                      {u"LightningResist"_s, 30},
                                                                      {u"LightningResistTotal"_s, 30},
                                                                      {u"MissingLightningResist"_s, 45},
                                                                      {u"MinionDPS"_s, 110000},
                                                                  }));
        QCOMPARE(v.verdict, Verdict::ResistLoss);
        QCOMPARE(v.breaks, QStringList{u"Lightning res goes further under the cap (from 38% to 30%)"_s});
    }

    void cappingRanksFirst()
    {
        const PobVerdict v = judgeWithPob(shortOfLightning, base);
        QCOMPARE(v.resistRank, 2);
        QCOMPARE(capping(v), QList<Element>{Element::Lightning});
        QCOMPARE(v.resistGain, 37);
        QCOMPARE(v.verdict, Verdict::Upgrade);

        // toward the cap without reaching it
        const PobVerdict some =
            judgeWithPob(shortOfLightning, with(shortOfLightning, {
                                                                      {u"LightningResist"_s, 50},
                                                                      {u"LightningResistTotal"_s, 50},
                                                                      {u"MissingLightningResist"_s, 25},
                                                                  }));
        QCOMPARE(some.resistRank, 1);
        QVERIFY(capping(some).isEmpty());
        QCOMPARE(some.score, 6.0);
    }

    void raisingTheCap()
    {
        // +1% to maximum lightning resistance without the resistance to fill it
        const PobStats atTheCap = with(base, {{u"LightningResistTotal"_s, 75}});
        const PobVerdict v = judgeWithPob(atTheCap, with(atTheCap, {{u"MissingLightningResist"_s, 1}}));
        QCOMPARE(v.verdict, Verdict::Same);
        QVERIFY(v.breaks.isEmpty());
        QCOMPARE(v.resistGain, 0);
        QCOMPARE(v.resistRank, 0);
    }

    void capWhenPathOfBuildingDoesntSay()
    {
        PobStats before = shortOfLightning;
        before.remove(u"MissingLightningResist"_s);
        PobStats after = with(before, {{u"LightningResist"_s, 80}, {u"LightningResistTotal"_s, 80}});

        // the other side's cap, else the usual one
        after.insert(u"MissingLightningResist"_s, 0);
        QCOMPARE(judgeWithPob(before, after).resists.first().max, 80.0);
        QCOMPARE(judgeWithPob(before, after).resistGain, 42);
        after.remove(u"MissingLightningResist"_s);
        QCOMPARE(judgeWithPob(before, after).resists.first().max, 75.0);
        QCOMPARE(judgeWithPob(before, after).resistGain, 37);

        // a total over the resistance shows it's held down to a cap, even under the usual one
        const PobStats under = {{u"ColdResist"_s, 60}, {u"ColdResistTotal"_s, 60}};
        const PobStats held = {{u"ColdResist"_s, 70}, {u"ColdResistTotal"_s, 80}};
        QCOMPARE(judgeWithPob(under, held).resistRank, 2);
        QVERIFY(judgeWithPob(held, under).resists.first().wasCapped);
    }

    void resistancesFirst()
    {
        // 37 lightning to the cap for a little minion damage, or more damage
        const PobStats fill = with(shortOfLightning, {
                                                         {u"LightningResist"_s, 75},
                                                         {u"LightningResistTotal"_s, 77},
                                                         {u"MissingLightningResist"_s, 0},
                                                         {u"MinionDPS"_s, 98000},
                                                     });
        const PobStats damage = with(shortOfLightning, {{u"MinionDPS"_s, 110000}});
        QVERIFY(judgeWithPob(shortOfLightning, fill).score > judgeWithPob(shortOfLightning, damage).score);

        PobPriorities damageFirst;
        damageFirst.resistsFirst = false;
        QVERIFY(judged(shortOfLightning, fill, damageFirst).score <
                judged(shortOfLightning, damage, damageFirst).score);
        // still counted, just not on the score
        QCOMPARE(judged(shortOfLightning, fill, damageFirst).resistGain, 37);
    }

    void chaosOnlyWhenAsked()
    {
        const PobStats chaos =
            with(base, {{u"ChaosResist"_s, 20}, {u"ChaosResistTotal"_s, 20}, {u"MissingChaosResist"_s, 55}});
        QCOMPARE(judgeWithPob(base, chaos).resistGain, 0);
        QCOMPARE(judgeWithPob(base, chaos).resists.size(), 1); // shown all the same

        PobPriorities withChaos;
        withChaos.chaos = true;
        QCOMPARE(judged(base, chaos, withChaos).resistGain, 20);

        // losing some under the cap doesn't matter then either
        QVERIFY(judgeWithPob(chaos, base).breaks.isEmpty());
        QCOMPARE(judged(chaos, base, withChaos).verdict, Verdict::ResistLoss);
    }

    void ignoringChaos()
    {
        // a Chaos Inoculation build with its chaos resistance capped anyway, and an item that loses it
        const PobStats ci = with(base, {{u"Life"_s, 1},
                                        {u"ChaosResist"_s, 75},
                                        {u"ChaosResistTotal"_s, 80},
                                        {u"MissingChaosResist"_s, 0}});
        const PobStats after = with(ci, {{u"ChaosResist"_s, 0},
                                         {u"ChaosResistTotal"_s, 0},
                                         {u"MissingChaosResist"_s, 75},
                                         {u"MinionDPS"_s, 110000}});
        PobVerdictOptions options;
        options.ignoreChaos = true;
        options.priorities.chaos = true;
        const PobVerdict v = judgeWithPob(ci, after, options);
        QCOMPARE(v.verdict, Verdict::Upgrade);
        QVERIFY(v.resists.isEmpty());
        // without it, the capped chaos resistance counts
        QCOMPARE(judgeWithPob(ci, after).verdict, Verdict::ResistLoss);
    }

    void chaosInoculation()
    {
        // the flag when Path of Building gives it
        QVERIFY(hasChaosInoculation({{u"ChaosInoculation"_s, 1}, {u"Life"_s, 1500}}));
        QVERIFY(!hasChaosInoculation({{u"ChaosInoculation"_s, 0}, {u"Life"_s, 1}}));
        // a saved build only has its Life to go by
        QVERIFY(hasChaosInoculation({{u"Life"_s, 1}}));
        QVERIFY(!hasChaosInoculation({{u"Life"_s, 1500}}));
        QVERIFY(!hasChaosInoculation({}));
    }

    void spiritAndAttributes()
    {
        const PobVerdict spirit =
            judgeWithPob(geared, with(geared, {{u"MinionDPS"_s, 120000}, {u"SpiritUnreserved"_s, -10}}));
        QCOMPARE(spirit.breaks, QStringList{u"Not enough Spirit for your minions and auras (10 short)"_s});
        QCOMPARE(spirit.verdict, Verdict::Risky);

        const PobVerdict strength =
            judgeWithPob(geared, with(geared, {{u"MinionDPS"_s, 120000}, {u"Str"_s, 60}}));
        QCOMPARE(strength.breaks, QStringList{u"Not enough Strength to use your gear (60 of 75)"_s});
        QCOMPARE(strength.verdict, Verdict::Risky);

        // and with nothing better about it, a downgrade
        QCOMPARE(judgeWithPob(geared, with(geared, {{u"Str"_s, 60}})).verdict, Verdict::Downgrade);
    }

    void shortfallsYouHadAlready()
    {
        // already short of Spirit, and the item doesn't make it worse
        const PobStats overReserved = with(geared, {{u"SpiritUnreserved"_s, -10}});
        QVERIFY(judgeWithPob(overReserved, overReserved).breaks.isEmpty());
        QCOMPARE(judgeWithPob(overReserved, with(geared, {{u"SpiritUnreserved"_s, -20}})).breaks.size(), 1);

        const PobStats weak = with(geared, {{u"Str"_s, 60}});
        QVERIFY(judgeWithPob(weak, with(weak, {{u"Str"_s, 55}})).breaks.isEmpty());
    }

    void switchingChecksOff()
    {
        PobPriorities relaxed;
        relaxed.spirit = false;
        relaxed.attributes = false;
        const PobStats after =
            with(geared, {{u"MinionDPS"_s, 120000}, {u"SpiritUnreserved"_s, -10}, {u"Str"_s, 60}});
        const PobVerdict v = judged(geared, after, relaxed);
        QVERIFY(v.breaks.isEmpty());
        QCOMPARE(v.verdict, Verdict::Upgrade);
    }

    void damageLoss()
    {
        const PobVerdict v = judgeWithPob(base, with(base, {{u"MinionDPS"_s, 90000}}));
        QCOMPARE(v.verdict, Verdict::DamageLoss);
        QVERIFY(v.losesDamage);
        QCOMPARE(v.resistRank, -1);
        QCOMPARE(v.breaks, QStringList{u"Minion damage drops 10%"_s});

        const PobVerdict own = judgeWithPob({{u"FullDPS"_s, 2000}}, {{u"FullDPS"_s, 1500}});
        QCOMPARE(own.breaks, QStringList{u"Damage drops 25%"_s});
    }

    void damageLossEvenWhenItCaps()
    {
        // a big part of the minions' damage for capping lightning and a lot more effective HP
        const PobStats before = {{u"MinionDPS"_s, 96800}, {u"TotalEHP"_s, 53000},     {u"FireResist"_s, 78},
                                 {u"ColdResist"_s, 75},   {u"LightningResist"_s, 38}, {u"ChaosResist"_s, 0},
                                 {u"Life"_s, 1}};
        const PobStats after =
            with(before, {{u"MinionDPS"_s, 77340}, {u"TotalEHP"_s, 67700}, {u"LightningResist"_s, 75}});
        PobVerdictOptions options;
        options.ignoreChaos = true;
        const PobVerdict v = judgeWithPob(before, after, options);
        QCOMPARE(v.verdict, Verdict::DamageLoss);
        QCOMPARE(v.resistRank, -1);
        QCOMPARE(v.damage.percent.value(), -20.1);
        QCOMPARE(v.breaks, QStringList{u"Minion damage drops 20.1%"_s});

        // a small dip is judged on the score
        QVERIFY(judgeWithPob(before, with(after, {{u"MinionDPS"_s, 95000}}), options).verdict !=
                Verdict::DamageLoss);
        // and so is any with the check off
        options.priorities.keepDamage = false;
        QCOMPARE(judgeWithPob(before, after, options).verdict, Verdict::Upgrade);
    }

    void resistLossOverDamageLoss()
    {
        const PobVerdict v = judgeWithPob(base, with(shortOfLightning, {{u"MinionDPS"_s, 50000}}));
        QCOMPARE(v.verdict, Verdict::ResistLoss);
        QVERIFY(v.losesDamage);
        QCOMPARE(v.breaks.size(), 2);
    }

    void otherNumbers()
    {
        const PobStats before = {{u"Life"_s, 1000},  {u"EnergyShield"_s, 2000},   {u"Mana"_s, 500},
                                 {u"Spirit"_s, 100}, {u"SpiritUnreserved"_s, 20}, {u"MinionLife"_s, 3000}};
        const PobStats after = {{u"Life"_s, 1000.4}, {u"EnergyShield"_s, 2100},   {u"Mana"_s, 500},
                                {u"Spirit"_s, 130},  {u"SpiritUnreserved"_s, 50}, {u"MinionLife"_s, 2900}};
        const PobVerdict v = judgeWithPob(before, after);
        QStringList stats;
        for (const PobChange &c : v.changes)
            stats << c.stat;
        QCOMPARE(stats,
                 (QStringList{u"EnergyShield"_s, u"Spirit"_s, u"SpiritUnreserved"_s, u"MinionLife"_s}));
        QCOMPARE(v.changes.first().from, 2000.0);
        QCOMPARE(v.changes.first().to, 2100.0);
        QCOMPARE(v.changes.first().percent.value(), 5.0);
        QCOMPARE(v.changes.last().percent.value(), -3.3);

        // from below 0 there's no percentage to give
        const PobVerdict reserved =
            judgeWithPob({{u"SpiritUnreserved"_s, -10}}, {{u"SpiritUnreserved"_s, 15}});
        QVERIFY(!reserved.changes.first().percent);
    }

    void bestOfSeveral()
    {
        const PobVerdict breaksSpirit =
            judgeWithPob(geared, with(geared, {{u"MinionDPS"_s, 130000}, {u"SpiritUnreserved"_s, -5}}));
        const PobVerdict clean = judgeWithPob(geared, with(geared, {{u"MinionDPS"_s, 104000}}));
        const PobVerdict better = judgeWithPob(geared, with(geared, {{u"MinionDPS"_s, 108000}}));
        QCOMPARE(bestVerdict({breaksSpirit, clean}), 1);
        QCOMPARE(bestVerdict({breaksSpirit, clean, better}), 2);
        QCOMPARE(bestVerdict({better, better}), 0);
        QCOMPARE(bestVerdict({}), -1);
    }
};

QTEST_GUILESS_MAIN(TestPobVerdict)
#include "tst_pobverdict.moc"
