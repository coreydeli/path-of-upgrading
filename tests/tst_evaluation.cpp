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

Item ring(const QString &name, const QString &mods)
{
    return item(u"Item Class: Rings\nRarity: Rare\n"_s + name + u"\nGold Ring\n--------\n"_s + mods);
}

const Item uniqueRing =
    item(u"Item Class: Rings\nRarity: Unique\nVenom Coil\nGold Ring\n--------\n+10 to maximum Life"_s);

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
        // lightning is right on the cap
        c.resists = resists({}, 60, 60, 75, 0);

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

    void weakestRingByGearScore()
    {
        // your weights only want rarity, so they'd have it replace the first
        Character c = wearing({
            {u"Ring 1"_s, ring(u"Storm Loop"_s, u"+70 to maximum Life"_s)},
            {u"Ring 2"_s, ring(u"Woe Band"_s, u"+10 to maximum Life\n20% increased Rarity of Items found"_s)},
        });
        c.weights = {{u"#% increased Rarity of Items found"_s, 10}};
        const Item better = ring(u"Grim Grasp"_s, u"+80 to maximum Life"_s);
        QCOMPARE(compare(better, c.gear, c.weights).targetSlot, u"Ring 1"_s);

        const Evaluation e = evaluate(better, c);
        QCOMPARE(e.comparison.targetSlot, u"Ring 2"_s);
        QCOMPARE(e.comparison.replacing, QStringList{u"Woe Band"_s});
        QCOMPARE(e.gearEquipped->score, 6);
        QCOMPARE(e.gear->score, 20);
        QCOMPARE(e.comparison.verdict, Verdict::Upgrade);
        // against the ring it replaces
        QCOMPARE(e.comparison.rows.at(0).key, u"# to maximum Life"_s);
        QCOMPARE(e.comparison.rows.at(0).oldValue, 10.0);
    }

    void scoredInTheSlotItTakes()
    {
        // Your weights only want rarity, so they'd have it replace the first
        // ring. You're capped on fire thanks to the second, so a ring's fire
        // resistance only counts in its place.
        const Equipped gear = {
            {u"Ring 1"_s, ring(u"Storm Loop"_s, u"+70 to maximum Life"_s)},
            {u"Ring 2"_s, ring(u"Woe Band"_s, u"+10 to maximum Life\n+30% to Fire Resistance"_s)},
        };
        Character c = wearing(gear);
        c.weights = {{u"#% increased Rarity of Items found"_s, 10}};
        c.resists = resists(gear, 75, {}, {}, {});
        const Item better = ring(u"Grim Grasp"_s, u"+40 to maximum Life\n+30% to Fire Resistance"_s);

        const Evaluation e = evaluate(better, c);
        QCOMPARE(e.comparison.targetSlot, u"Ring 2"_s);
        QCOMPARE(e.gearEquipped->score, 14);
        QCOMPARE(e.gear->score, 22);
        QCOMPARE(e.comparison.verdict, Verdict::Upgrade);
        QCOMPARE(evaluate(better, c, {}, u"Ring 1"_s).gear->score, 11);
    }

    void uniqueRingIsntTheWeakest()
    {
        const Character c = wearing({
            {u"Ring 1"_s, uniqueRing},
            {u"Ring 2"_s, ring(u"Woe Band"_s, u"+20 to maximum Life"_s)},
        });
        const Evaluation e = evaluate(ring(u"Grim Grasp"_s, u"+40 to maximum Life"_s), c);
        QCOMPARE(e.comparison.targetSlot, u"Ring 2"_s);
        QCOMPARE(e.mode, ScoreMode::GearScores);
        QCOMPARE(e.comparison.delta, 5.0);
        QCOMPARE(e.comparison.verdict, Verdict::Small);
    }

    void freeRingSlotFirst()
    {
        const Character c = wearing({{u"Ring 1"_s, ring(u"Woe Band"_s, u"+10 to maximum Life"_s)}});
        const Evaluation e = evaluate(ring(u"Grim Grasp"_s, u"+40 to maximum Life"_s), c);
        QCOMPARE(e.comparison.targetSlot, u"Ring 2"_s);
        QCOMPARE(e.comparison.verdict, Verdict::New);
    }

    void ringSlotYouName()
    {
        const Character c = wearing({
            {u"Ring 1"_s, ring(u"Storm Loop"_s, u"+70 to maximum Life"_s)},
            {u"Ring 2"_s, ring(u"Woe Band"_s, u"+10 to maximum Life"_s)},
        });
        const Evaluation e = evaluate(ring(u"Grim Grasp"_s, u"+80 to maximum Life"_s), c, {}, u"Ring 1"_s);
        QCOMPARE(e.comparison.targetSlot, u"Ring 1"_s);
        QCOMPARE(e.comparison.delta, 2.0);
        QCOMPARE(e.comparison.verdict, Verdict::Same);
    }

    void ringYouWear()
    {
        // and not your weakest
        const Character c = wearing({
            {u"Ring 1"_s, ring(u"Woe Band"_s, u"+10 to maximum Life"_s)},
            {u"Ring 2"_s, ring(u"Storm Loop"_s, u"+70 to maximum Life"_s)},
        });
        const Evaluation e = evaluate(c.gear.value(u"Ring 2"_s), c);
        QCOMPARE(e.comparison.verdict, Verdict::Equipped);
        QCOMPARE(e.comparison.targetSlot, u"Ring 2"_s);
    }

    void rowsByWhatTheyreWorth()
    {
        const Character c = wearing({{u"Amulet"_s, lifeAmulet(20)}});
        const Item better =
            amulet(u"+40 to maximum Life\n+50 to maximum Mana\n30% increased Rarity of Items found"_s);

        // to the build, where mana is worth nothing
        const Evaluation e = evaluate(better, c);
        QCOMPARE(e.mode, ScoreMode::GearScores);
        const QList<StatRow> &rows = e.comparison.rows;
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.at(0).key, u"#% increased Rarity of Items found"_s);
        QCOMPARE(rows.at(0).usefulness, 0.05);
        QCOMPARE(rows.at(1).key, u"# to maximum Life"_s);
        QCOMPARE(rows.at(1).usefulness, 0.07);
        QCOMPARE(rows.at(2).key, u"# to maximum Mana"_s);
        QCOMPARE(rows.at(2).usefulness, 0.0);

        // by your weights, which don't have rarity
        Character noProfile = c;
        noProfile.buildProfile = u"none of them"_s;
        const QList<StatRow> byWeight = evaluate(better, noProfile).comparison.rows;
        QCOMPARE(byWeight.at(0).key, u"# to maximum Life"_s);
        QCOMPARE(byWeight.at(1).key, u"# to maximum Mana"_s);
        QCOMPARE(byWeight.at(1).usefulness, byWeight.at(1).weight);
        QCOMPARE(byWeight.at(2).key, u"#% increased Rarity of Items found"_s);
    }

    void whatEquippingChanges()
    {
        const Character c = wearing({{u"Amulet"_s, lifeAmulet(40)}, {u"Ring 1"_s, uniqueRing}});
        const Evaluation e = evaluate(lifeAmulet(64), c);
        QVERIFY(e.change);
        QCOMPARE(e.change->targets, QStringList{u"Amulet"_s});
        QCOMPARE(e.change->totals.size(), 1);
        QCOMPARE(e.change->totals.at(0).label, u"Life"_s);
        QCOMPARE(e.change->totals.at(0).delta, 24.0);
        // the average over your main slots, where the unique has no gear score
        QCOMPARE(e.change->overall->from, double(e.gearEquipped->score));
        QCOMPARE(e.change->overall->to, double(e.gear->score));
        QVERIFY(e.risks.isEmpty());

        QVERIFY(!evaluate(lifeAmulet(40), c).change);
    }

    void twoHanderTakesTheOffhandsPlace()
    {
        const Item mace =
            item(u"Item Class: One Hand Maces\nRarity: Rare\nDoom Knell\nTorment Club\n--------\n"
                 "Physical Damage: 10-20\nAttacks per Second: 1.50\n--------\n+30 to maximum Life"_s);
        const Item shield = item(u"Item Class: Shields\nRarity: Rare\nWoe Guard\nPainted Tower Shield\n"
                                 "--------\nArmour: 200\n--------\n+50 to maximum Life"_s);
        const Item maul =
            item(u"Item Class: Two Hand Maces\nRarity: Rare\nBrute Knell\nForge Maul\n--------\n"
                 "Physical Damage: 100-160\nAttacks per Second: 1.10\n--------\n+40 to Strength"_s);
        const Evaluation e = evaluate(maul, wearing({{u"Weapon"_s, mace}, {u"Offhand"_s, shield}}));
        QCOMPARE(e.change->targets, (QStringList{u"Weapon"_s, u"Offhand"_s}));
        QCOMPARE(e.change->totals.at(0).label, u"Life"_s);
        QCOMPARE(e.change->totals.at(0).delta, -80.0);
    }

    void losingSpiritIsARisk_data()
    {
        QTest::addColumn<int>("life");
        QTest::addColumn<Verdict>("verdict");
        QTest::addColumn<Verdict>("otherwise");

        QTest::newRow("upgrade") << 64 << Verdict::Risky << Verdict::Upgrade;
        QTest::newRow("small upgrade") << 40 << Verdict::Risky << Verdict::Small;
        QTest::newRow("no better") << 20 << Verdict::Same << Verdict::Same;
    }

    void losingSpiritIsARisk()
    {
        QFETCH(int, life);
        QFETCH(Verdict, verdict);
        QFETCH(Verdict, otherwise);

        const Character c = wearing({{u"Amulet"_s, amulet(u"+20 to maximum Life\n+30 to Spirit"_s)}});
        const Evaluation e = evaluate(lifeAmulet(life), c);
        QCOMPARE(e.risks, QStringList{u"Spirit drops by 30, so check your minions and auras still fit"_s});
        QCOMPARE(e.comparison.verdict, verdict);

        // when you've said Spirit doesn't matter to you
        const Evaluation anyway = evaluate(lifeAmulet(life), c, {.spirit = false});
        QVERIFY(anyway.risks.isEmpty());
        QCOMPARE(anyway.comparison.verdict, otherwise);
    }

    void losingMinionLevelsIsARisk_data()
    {
        QTest::addColumn<QString>("mod");
        QTest::addColumn<QString>("risk");

        QTest::newRow("one") << u"+1 to Level of all Minion Skills"_s << u"Minion skills lose a level"_s;
        QTest::newRow("two") << u"+2 to Level of all Minion Skills"_s << u"Minion skills lose 2 levels"_s;
    }

    void losingMinionLevelsIsARisk()
    {
        QFETCH(QString, mod);
        QFETCH(QString, risk);

        const Character c = wearing({{u"Amulet"_s, amulet(u"+20 to maximum Life\n"_s + mod)}});
        const Evaluation e = evaluate(lifeAmulet(64), c);
        QCOMPARE(e.risks, QStringList{risk});
        QCOMPARE(e.comparison.verdict, Verdict::Risky);
    }

    void resistanceWarningsAreRisks()
    {
        // only just over the cap without it
        const Equipped gear = {{u"Amulet"_s, amulet(u"+20 to maximum Life\n+10% to Fire Resistance"_s)}};
        Character c = wearing(gear);
        c.resists = resists(gear, 90, {}, {}, {});
        const Evaluation e = evaluate(lifeAmulet(64), c);
        QCOMPARE(e.risks.size(), 1);
        QVERIFY(e.risks.at(0).startsWith(u"Fire res would be only 5% over the cap"));
        QCOMPARE(e.comparison.verdict, Verdict::Risky);
    }

    void losingResistanceYouNeed()
    {
        const Equipped gear = {{u"Amulet"_s, amulet(u"+20 to maximum Life\n+20% to Fire Resistance"_s)}};
        Character c = wearing(gear);
        c.resists = resists(gear, 80, {}, {}, {});
        const Evaluation e = evaluate(lifeAmulet(64), c);
        QCOMPARE(e.comparison.verdict, Verdict::ResistLoss);
        QCOMPARE(e.resistLosses.size(), 1);
        QCOMPARE(e.resistLosses.at(0).element, Element::Fire);
        QCOMPARE(e.resistLosses.at(0).to, 60.0);
        QCOMPARE(e.resistRank, -1);
        QCOMPARE(e.risks, QStringList{u"Fire res drops below the cap (60% of 75%)"_s});
    }

    void resistanceRanks_data()
    {
        QTest::addColumn<Element>("element");
        QTest::addColumn<double>("from");
        QTest::addColumn<bool>("priority");
        QTest::addColumn<int>("rank");

        QTest::newRow("caps one") << Element::Fire << 70.0 << false << 2;
        QTest::newRow("adds toward a cap") << Element::Fire << 50.0 << false << 1;
        QTest::newRow("already capped") << Element::Fire << 90.0 << false << 0;
        QTest::newRow("chaos you don't want") << Element::Chaos << 30.0 << false << 0;
        QTest::newRow("chaos you want") << Element::Chaos << 30.0 << true << 1;
    }

    void resistanceRanks()
    {
        QFETCH(Element, element);
        QFETCH(double, from);
        QFETCH(bool, priority);
        QFETCH(int, rank);

        const Equipped gear = {{u"Amulet"_s, lifeAmulet(20)}};
        Character c = wearing(gear);
        PerElement<std::optional<double>> values;
        values[element] = from;
        c.resists = resists(gear, values[Element::Fire], {}, {}, values[Element::Chaos]);
        const Item better = amulet(u"+20 to maximum Life\n+10% to %1 Resistance"_s.arg(elementName(element)));
        const Evaluation e = evaluate(better, c, {.chaos = priority});
        QCOMPARE(e.resistRank, rank);
        QVERIFY(e.resistLosses.isEmpty());
    }

    void chaosResistanceRisks_data()
    {
        QTest::addColumn<double>("chaos");
        QTest::addColumn<bool>("priority");
        QTest::addColumn<bool>("inoculation");
        QTest::addColumn<bool>("risk");

        // dropping off a cap you had always matters
        QTest::newRow("capped") << 80.0 << false << false << true;
        QTest::newRow("under") << 30.0 << false << false << false;
        QTest::newRow("under, and you want it") << 30.0 << true << false << true;
        QTest::newRow("Chaos Inoculation") << 80.0 << true << true << false;
    }

    void chaosResistanceRisks()
    {
        QFETCH(double, chaos);
        QFETCH(bool, priority);
        QFETCH(bool, inoculation);
        QFETCH(bool, risk);

        const Equipped gear = {{u"Amulet"_s, amulet(u"+20 to maximum Life\n+13% to Chaos Resistance"_s)}};
        Character c = wearing(gear);
        c.resists = resists(gear, {}, {}, {}, chaos);
        c.chaosInoculation = inoculation;
        const Evaluation e = evaluate(lifeAmulet(64), c, {.chaos = priority});
        QCOMPARE(!e.risks.isEmpty(), risk);
        QCOMPARE(e.comparison.verdict == Verdict::ResistLoss, risk);
    }

    void pathOfBuildingsJudgement()
    {
        const Character c = wearing({{u"Amulet"_s, lifeAmulet(40)}});
        PobJudgement j;
        j.placement = {.slot = u"Amulet"_s, .label = u"Amulet"_s, .replacing = u"Doom Charm, Gold Amulet"_s};
        j.verdict.verdict = Verdict::Risky;
        j.verdict.score = 4.2;
        j.verdict.breaks = {u"Not enough Dexterity to use your gear (80 of 95)"_s};
        j.verdict.resistRank = 1;

        const Evaluation e = withPob(evaluate(lifeAmulet(64), c), j);
        QCOMPARE(e.mode, ScoreMode::PathOfBuilding);
        QCOMPARE(e.comparison.verdict, Verdict::Risky);
        QCOMPARE(e.comparison.delta, 4.2);
        QVERIFY(!e.comparison.percent);
        QCOMPARE(e.risks, j.verdict.breaks);
        QCOMPARE(e.resistRank, 1);
        QCOMPARE(e.comparison.replacing, QStringList{u"Doom Charm"_s});
        QCOMPARE(e.pob->placement.slot, u"Amulet"_s);
        // still there to show
        QVERIFY(e.gear);
        QVERIFY(e.change);
    }

    void pathOfBuildingKnowsYourResistances()
    {
        const Equipped gear = {{u"Amulet"_s, amulet(u"+20 to maximum Life\n+20% to Fire Resistance"_s)}};
        Character c = wearing(gear);
        c.resists = resists(gear, 80, {}, {}, {});
        const Evaluation ours = evaluate(lifeAmulet(64), c);
        QCOMPARE(ours.comparison.verdict, Verdict::ResistLoss);

        // its tree makes up for the amulet
        PobJudgement j;
        j.verdict.verdict = Verdict::Upgrade;
        j.verdict.score = 6;
        const Evaluation e = withPob(ours, j);
        QCOMPARE(e.comparison.verdict, Verdict::Upgrade);
        QVERIFY(e.resistLosses.isEmpty());
        QVERIFY(e.risks.isEmpty());
        QCOMPARE(e.resistRank, 0);
    }

    void pathOfBuildingOverYourWeights()
    {
        Character c = wearing({{u"Amulet"_s, lifeAmulet(40)}});
        c.buildProfile = u"none of them"_s;
        const Evaluation ours = evaluate(lifeAmulet(64), c);
        QVERIFY(ours.comparison.percent);

        PobJudgement j;
        j.verdict.verdict = Verdict::Small;
        j.verdict.score = 1.5;
        const Evaluation e = withPob(ours, j);
        QCOMPARE(e.mode, ScoreMode::PathOfBuilding);
        QCOMPARE(e.comparison.verdict, Verdict::Small);
        QCOMPARE(e.comparison.delta, 1.5);
        // the delta is a percentage already
        QVERIFY(!e.comparison.percent);
    }

    void jewelSockets()
    {
        const Item jewel =
            item(u"Item Class: Jewels\nRarity: Rare\nGrim Eye\nSapphire\n--------\n+20 to maximum Life"_s);
        const Evaluation ours = evaluate(jewel, {});
        QCOMPARE(ours.comparison.targetSlot, u"Jewel 1"_s);

        PobJudgement j;
        j.placement = {.slot = u"Jewel 61834"_s, .label = u"Socket #3"_s, .replacing = {}};
        j.verdict.verdict = Verdict::Upgrade;
        QCOMPARE(withPob(ours, j).comparison.replacing, QStringList{u"an empty jewel socket (Socket #3)"_s});
        j.placement.replacing = u"Doom Eye, Ruby"_s;
        QCOMPARE(withPob(ours, j).comparison.replacing, QStringList{u"Doom Eye, Ruby (Socket #3)"_s});
    }

    void pathOfBuildingOnAnItemYouWear()
    {
        const Character c = wearing({{u"Amulet"_s, lifeAmulet(40)}});
        PobJudgement j;
        j.verdict.verdict = Verdict::Same;
        const Evaluation e = withPob(evaluate(lifeAmulet(40), c), j);
        QCOMPARE(e.comparison.verdict, Verdict::Equipped);
        QCOMPARE(e.mode, ScoreMode::GearScores);
        QVERIFY(!e.pob);
    }

    void notGear()
    {
        const Evaluation e = evaluate(
            item(
                u"Item Class: Stackable Currency\nRarity: Currency\nDivine Orb\n--------\nStack Size: 1/20"_s),
            {});
        QVERIFY(!e.comparison.ok);
        QVERIFY(!e.gear);
        QVERIFY(!e.change);

        // and there's nothing for Path of Building to judge
        PobJudgement j;
        j.verdict.verdict = Verdict::Upgrade;
        const Evaluation pob = withPob(e, j);
        QVERIFY(!pob.comparison.ok);
        QCOMPARE(pob.mode, ScoreMode::StatWeights);
        QVERIFY(!pob.pob);
    }
};

QTEST_GUILESS_MAIN(TestEvaluation)
#include "tst_evaluation.moc"
