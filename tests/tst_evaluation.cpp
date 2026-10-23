// SPDX-License-Identifier: MIT
#include "core/evaluation.h"
#include "core/parser.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

Item amulet(const QString &mods)
{
    return item(u"Item Class: Amulets\nRarity: Rare\nDoom Charm\nGold Amulet\n--------\n"_s + mods);
}

Item lifeAmulet(int life)
{
    return amulet(u"+%1 to maximum Life"_s.arg(life));
}

const Item uniqueAmulet = item(
    u"Item Class: Amulets\nRarity: Unique\nHollow Promise\nGold Amulet\n--------\n+10 to maximum Life"_s);

Character wearing(const Equipped &gear)
{
    Character c;
    c.gear = gear;
    return c;
}

// Your resistances as you'd type them in, with the gear you wear now.
ResistRecord resists(const Equipped &gear, std::optional<double> fire, std::optional<double> cold,
                     std::optional<double> lightning, std::optional<double> chaos)
{
    ResistEntry entry;
    entry.values[Element::Fire] = fire;
    entry.values[Element::Cold] = cold;
    entry.values[Element::Lightning] = lightning;
    entry.values[Element::Chaos] = chaos;
    entry.buffer = 10;
    return snapshotResists(entry, gear, ResistSource::Manual);
}

} // namespace

class TestEvaluation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void gearScoreOptionsFromTheCharacter()
    {
        Character c;
        c.buildProfile = u"es-caster"_s;
        c.tweaks = {{u"Life"_s, 1.5}};
        c.ignored = {IgnoreRule{.key = u"# to maximum Mana"_s}};

        const GearScoreOptions options = gearScoreOptions(c, u"Helmet"_s);
        QCOMPARE(options.build, u"es-caster"_s);
        QCOMPARE(options.tweaks, (ProfileTweaks{{u"Life"_s, 1.5}}));
        QVERIFY(options.ignore(u"# to maximum Mana"_s));
        QVERIFY(!options.ignore(u"# to maximum Life"_s));
        // without resistances there's no telling what you need
        QVERIFY(!options.resistNeed[Element::Fire]);
    }

    void resistancesYouStillNeed()
    {
        Character c;
        c.gear = {{u"Amulet"_s, amulet(u"+20% to Fire Resistance"_s)}};
        c.resists = resists(c.gear, 60, 90, std::nullopt, 0);

        // the amulet's fire resistance goes when another takes its place
        const GearScoreOptions options = gearScoreOptions(c, u"Amulet"_s, {.resistsFirst = false});
        QCOMPARE(options.resistNeed[Element::Fire], 45.0);
        QCOMPARE(options.resistNeed[Element::Cold], 0.0);
        QVERIFY(!options.resistNeed[Element::Lightning]);
        QCOMPARE(options.resistNeed[Element::Chaos], 85.0);
        QCOMPARE(gearScoreOptions(c, u"Helmet"_s).resistNeed[Element::Fire], 25.0);
    }

    void resistancesFirst()
    {
        Character c;
        c.tweaks = {{u"Cold res"_s, 0.5}};
        c.resists = resists({}, 60, 60, 80, 0);

        // those you're short of, unless you tuned them yourself
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s).tweaks,
                 (ProfileTweaks{{u"Fire res"_s, 2}, {u"Cold res"_s, 0.5}}));
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s, {.chaos = true}).tweaks,
                 (ProfileTweaks{{u"Fire res"_s, 2}, {u"Cold res"_s, 0.5}, {u"Chaos res"_s, 2}}));
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s, {.resistsFirst = false}).tweaks,
                 (ProfileTweaks{{u"Cold res"_s, 0.5}}));
    }

    void chaosInoculation()
    {
        Character c;
        c.chaosInoculation = true;
        c.resists = resists({}, 80, 80, 80, 0);
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s, {.chaos = true}).tweaks,
                 (ProfileTweaks{{u"Chaos res"_s, 0}}));

        c.tweaks = {{u"Chaos res"_s, 1}};
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s).tweaks, (ProfileTweaks{{u"Chaos res"_s, 1}}));
    }

    void byGearScore_data()
    {
        QTest::addColumn<int>("yours");
        QTest::addColumn<int>("life");
        QTest::addColumn<Verdict>("verdict");

        // an amulet with nothing but life scores about a point for every 4 of it
        QTest::newRow("upgrade") << 40 << 64 << Verdict::Upgrade;
        QTest::newRow("small upgrade") << 40 << 60 << Verdict::Small;
        QTest::newRow("just a small one") << 40 << 52 << Verdict::Small;
        QTest::newRow("too little to tell") << 40 << 48 << Verdict::Same;
        QTest::newRow("too little either way") << 40 << 32 << Verdict::Same;
        QTest::newRow("downgrade") << 40 << 28 << Verdict::Downgrade;
    }

    void byGearScore()
    {
        QFETCH(int, yours);
        QFETCH(int, life);
        QFETCH(Verdict, verdict);

        const Character c = wearing({{u"Amulet"_s, lifeAmulet(yours)}});
        const Evaluation e = evaluate(lifeAmulet(life), c);
        QCOMPARE(e.mode, ScoreMode::GearScores);
        QCOMPARE(e.comparison.verdict, verdict);
        QCOMPARE(e.comparison.newScore, double(e.gear->score));
        QCOMPARE(e.comparison.oldScore, double(e.gearEquipped->score));
        QCOMPARE(e.comparison.delta, double(e.gear->score - e.gearEquipped->score));
        QVERIFY(!e.comparison.percent);
    }

    void intoAnEmptySlot()
    {
        const Evaluation e = evaluate(lifeAmulet(60), {});
        QCOMPARE(e.mode, ScoreMode::GearScores);
        QCOMPARE(e.comparison.verdict, Verdict::New);
        QCOMPARE(e.comparison.delta, double(e.gear->score));
        QVERIFY(!e.gearEquipped);
    }

    void itemYouWear()
    {
        const Evaluation e = evaluate(lifeAmulet(60), wearing({{u"Amulet"_s, lifeAmulet(60)}}));
        QCOMPARE(e.comparison.verdict, Verdict::Equipped);
        QCOMPARE(e.comparison.delta, 0.0);
        QVERIFY(!e.gearEquipped);
    }

    void byWeightsWhenYoursHasNoGearScore()
    {
        const Evaluation e = evaluate(lifeAmulet(60), wearing({{u"Amulet"_s, uniqueAmulet}}));
        QCOMPARE(e.mode, ScoreMode::StatWeights);
        QVERIFY(e.gear->isRated());
        QCOMPARE(e.gearEquipped->unrated, GearScore::Unrated::Unique);
        QCOMPARE(e.comparison.delta, 50.0);
        QVERIFY(e.comparison.percent);
        QCOMPARE(e.comparison.verdict, Verdict::Upgrade);
    }

    void byWeightsForAUnique()
    {
        const Evaluation e = evaluate(uniqueAmulet, wearing({{u"Amulet"_s, lifeAmulet(60)}}));
        QCOMPARE(e.mode, ScoreMode::StatWeights);
        QCOMPARE(e.gear->unrated, GearScore::Unrated::Unique);
        QCOMPARE(e.comparison.verdict, Verdict::Downgrade);
    }

    void byWeightsWithoutABuildProfile()
    {
        Character c = wearing({{u"Amulet"_s, lifeAmulet(40)}});
        c.buildProfile = u"none of them"_s;
        const Evaluation e = evaluate(lifeAmulet(44), c);
        QCOMPARE(e.mode, ScoreMode::StatWeights);
        QVERIFY(!e.gear);
        QVERIFY(!e.gearEquipped);
        QCOMPARE(e.comparison.verdict, Verdict::Upgrade);
    }

    void notGear()
    {
        const Evaluation e = evaluate(
            item(
                u"Item Class: Stackable Currency\nRarity: Currency\nDivine Orb\n--------\nStack Size: 1/20"_s),
            {});
        QVERIFY(!e.comparison.ok);
        QVERIFY(!e.gear);
    }
};

QTEST_GUILESS_MAIN(TestEvaluation)
#include "tst_evaluation.moc"
