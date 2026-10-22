// SPDX-License-Identifier: MIT
#include "core/pobverdict.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// A minion build.
const PobStats base = {
    {u"FullDPS"_s, 0},
    {u"MinionDPS"_s, 100000},
    {u"TotalEHP"_s, 50000},
};

PobStats with(PobStats stats, const PobStats &changes)
{
    stats.insert(changes);
    return stats;
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

    void damageLoss()
    {
        const PobVerdict v = judgeWithPob(base, with(base, {{u"MinionDPS"_s, 90000}}));
        QCOMPARE(v.verdict, Verdict::DamageLoss);
        QVERIFY(v.losesDamage);
        QCOMPARE(v.breaks, QStringList{u"Minion damage drops 10%"_s});

        const PobVerdict own = judgeWithPob({{u"FullDPS"_s, 2000}}, {{u"FullDPS"_s, 1500}});
        QCOMPARE(own.breaks, QStringList{u"Damage drops 25%"_s});
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
};

QTEST_GUILESS_MAIN(TestPobVerdict)
#include "tst_pobverdict.moc"
