// SPDX-License-Identifier: MIT
#include "app/itemreport.h"
#include "core/parser.h"
#include "itemtexts.h"

#include <QTest>

using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

const Item helmetNew = item(itemtexts::helmetNew);
const Item helmetOld = item(itemtexts::helmetOld);

Character wearing(const Equipped &gear)
{
    Character c;
    c.gear = gear;
    return c;
}

// The report on an item as the judge has it straight away.
ItemReport reportOn(const Item &it, const Character &c = {})
{
    return reportOf({.item = it, .evaluation = evaluate(it, c)});
}

// The new helmet, judged as you say against the old one.
ItemCheck judged(Verdict verdict, ScoreMode mode = ScoreMode::GearScores, double delta = 0)
{
    ItemCheck check{.item = helmetNew};
    Comparison &r = check.evaluation.comparison;
    r.ok = true;
    r.verdict = verdict;
    r.targetSlot = u"Helmet"_s;
    r.replacing = {u"Grim Crown"_s};
    r.delta = delta;
    r.isEquipped = verdict == Verdict::Equipped;
    check.evaluation.mode = mode;
    return check;
}

// The new helmet as Path of Building judged it.
ItemCheck judgedByPob(const PobVerdict &verdict)
{
    PobJudgement j;
    j.verdict = verdict;
    ItemCheck check = judged(verdict.verdict);
    check.evaluation = withPob(check.evaluation, j);
    check.pob = ItemCheck::Pob::Checked;
    return check;
}

} // namespace

class TestItemReport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theItem()
    {
        const ItemReport report = reportOn(helmetNew);
        QCOMPARE(report.name, u"Doom Visage"_s);
        QCOMPARE(report.rarity, u"Rare"_s);
        QVERIFY(report.gear);
        QCOMPARE(report.details, (QStringList{u"Expert Visored Helm"_s, u"Helmets"_s, u"Item level 79"_s}));
    }

    void namedAfterItsBase()
    {
        const ItemReport report =
            reportOn(item(u"Item Class: Rings\nRarity: Normal\nGold Ring\n--------\n+10 to maximum Life"_s));
        QCOMPARE(report.name, u"Gold Ring"_s);
        QCOMPARE(report.details, QStringList{u"Rings"_s});
    }

    void openAffixes_data()
    {
        QTest::addColumn<int>("prefixes");
        QTest::addColumn<int>("suffixes");
        QTest::addColumn<QString>("open");

        QTest::newRow("both") << 1 << 2 << u"Open affixes: 1 prefix, 2 suffixes"_s;
        QTest::newRow("prefixes") << 2 << 0 << u"Open affixes: 2 prefixes"_s;
        QTest::newRow("a suffix") << 0 << 1 << u"Open affixes: 1 suffix"_s;
        QTest::newRow("none") << 0 << 0 << u"No open affixes"_s;
    }

    void openAffixes()
    {
        QFETCH(int, prefixes);
        QFETCH(int, suffixes);
        QFETCH(QString, open);

        Item it = helmetNew;
        it.affixes = {.known = true, .openPrefix = prefixes, .openSuffix = suffixes};
        QCOMPARE(reportOn(it).openAffixes, open);
    }

    void affixesUnknown()
    {
        // from a plain copy
        QCOMPARE(reportOn(helmetNew).openAffixes, QString());
    }

    void verdicts_data()
    {
        QTest::addColumn<Verdict>("verdict");
        QTest::addColumn<QString>("text");
        QTest::addColumn<Tone::Tone>("tone");

        QTest::newRow("new") << Verdict::New << u"Empty slot"_s << Tone::Plain;
        QTest::newRow("same") << Verdict::Same << u"Sidegrade"_s << Tone::Plain;
        QTest::newRow("small") << Verdict::Small << u"Small upgrade"_s << Tone::Good;
        QTest::newRow("upgrade") << Verdict::Upgrade << u"Upgrade"_s << Tone::Good;
        QTest::newRow("downgrade") << Verdict::Downgrade << u"Downgrade"_s << Tone::Bad;
        QTest::newRow("risky") << Verdict::Risky << u"Risky upgrade"_s << Tone::Warning;
        QTest::newRow("resistance") << Verdict::ResistLoss << u"Loses resistance"_s << Tone::Bad;
        QTest::newRow("damage") << Verdict::DamageLoss << u"Loses damage"_s << Tone::Bad;
        QTest::newRow("equipped") << Verdict::Equipped << u"Equipped"_s << Tone::Plain;
    }

    void verdicts()
    {
        QFETCH(Verdict, verdict);
        QFETCH(QString, text);
        QFETCH(Tone::Tone, tone);

        const ItemReport report = reportOf(judged(verdict));
        QCOMPARE(report.verdict, text);
        QCOMPARE(report.verdictTone, tone);
    }

    void scores_data()
    {
        QTest::addColumn<ScoreMode>("mode");
        QTest::addColumn<Verdict>("verdict");
        QTest::addColumn<double>("delta");
        QTest::addColumn<std::optional<double>>("percent");
        QTest::addColumn<QString>("score");
        QTest::addColumn<QString>("label");

        using enum ScoreMode;
        const std::optional<double> none;
        QTest::newRow("path of building")
            << PathOfBuilding << Verdict::Upgrade << 4.0 << none << u"+4%"_s << u"for your build"_s;
        QTest::newRow("path of building, worse")
            << PathOfBuilding << Verdict::Downgrade << -2.5 << none << u"-2.5%"_s << u"for your build"_s;
        // which would read like an upgrade
        QTest::newRow("losing resistance for more")
            << PathOfBuilding << Verdict::ResistLoss << 6.0 << none << QString() << QString();
        QTest::newRow("losing damage for less")
            << PathOfBuilding << Verdict::DamageLoss << -3.0 << none << u"-3%"_s << u"for your build"_s;
        QTest::newRow("gear score") << GearScores << Verdict::Upgrade << 6.0 << none << u"+6"_s
                                    << u"gear score"_s;
        QTest::newRow("gear score, the same")
            << GearScores << Verdict::Same << 0.0 << none << u"0"_s << u"gear score"_s;
        QTest::newRow("gear score, losing resistance")
            << GearScores << Verdict::ResistLoss << 8.0 << none << u"+8"_s << u"gear score"_s;
        QTest::newRow("weights") << StatWeights << Verdict::Upgrade << 12.54 << std::optional(3.04)
                                 << u"+12.5"_s << u"by your weights (+3%)"_s;
        QTest::newRow("weights, from nothing")
            << StatWeights << Verdict::Upgrade << 12.0 << none << u"+12"_s << u"by your weights"_s;
        QTest::newRow("weights, next to nothing")
            << StatWeights << Verdict::Same << -0.04 << std::optional(-0.2) << u"0"_s
            << u"by your weights (-0.2%)"_s;
    }

    void scores()
    {
        QFETCH(ScoreMode, mode);
        QFETCH(Verdict, verdict);
        QFETCH(double, delta);
        QFETCH(std::optional<double>, percent);
        QFETCH(QString, score);
        QFETCH(QString, label);

        ItemCheck check = judged(verdict, mode, delta);
        check.evaluation.comparison.percent = percent;
        const ItemReport report = reportOf(check);
        QCOMPARE(report.score, score);
        QCOMPARE(report.scoreLabel, label);
    }

    void nothingToScoreAgainst()
    {
        ItemReport report = reportOn(helmetNew);
        QCOMPARE(report.verdict, u"Empty slot"_s);
        QCOMPARE(report.score, QString());
        QCOMPARE(report.versus, QString());
        QVERIFY(report.notes.contains(
            u"Nothing saved for Helmet yet. Copy the item you wear there and set it as equipped."_s));

        report = reportOn(helmetOld, wearing({{u"Helmet"_s, helmetOld}}));
        QCOMPARE(report.verdict, u"Equipped"_s);
        QCOMPARE(report.score, QString());
        QCOMPARE(report.versus, u"in your Helmet slot"_s);
    }

    void emptySlotForPathOfBuilding()
    {
        ItemCheck check = judged(Verdict::Upgrade, ScoreMode::PathOfBuilding, 10);
        check.evaluation.comparison.replacing.clear();
        QCOMPARE(reportOf(check).score, u"+10%"_s);
    }

    void versus()
    {
        QCOMPARE(reportOn(helmetNew, wearing({{u"Helmet"_s, helmetOld}})).versus, u"vs Grim Crown"_s);

        // a two-handed weapon for your weapon and shield
        const Item mace = item(u"Item Class: One Hand Maces\nRarity: Rare\nGrim Knell\nWar Hammer\n--------\n"
                               "Physical Damage: 10-20\nAttacks per Second: 1.45"_s);
        const Item shield =
            item(u"Item Class: Shields\nRarity: Rare\nBone Wall\nTower Shield\n--------\nArmour: 100"_s);
        const Character c = wearing({{u"Weapon"_s, mace}, {u"Offhand"_s, shield}});
        QCOMPARE(reportOn(item(itemtexts::twoHandMace), c).versus, u"vs Grim Knell and Bone Wall"_s);
    }

    void uniques()
    {
        const Item unique = item(
            u"Item Class: Helmets\nRarity: Unique\nQuiet Hollow\nVisored Helm\n--------\n+40 to maximum Life"_s);
        ItemCheck check{.item = unique, .evaluation = evaluate(unique, wearing({{u"Helmet"_s, helmetOld}}))};
        ItemReport report = reportOf(check);
        QCOMPARE(report.verdict, u"Unique"_s);
        QCOMPARE(report.verdictTone, Tone::Plain);
        QCOMPARE(report.score, QString());
        QCOMPARE(report.versus, u"vs Grim Crown"_s);
        const QString fixed = u"Uniques have fixed mods, so they don't get a gear score."_s;
        QVERIFY(report.notes.contains(fixed));

        // which Path of Building is about to judge
        check.pob = ItemCheck::Pob::Checking;
        QVERIFY(!reportOf(check).notes.contains(fixed));

        check.pob = ItemCheck::Pob::Checked;
        check.evaluation.mode = ScoreMode::PathOfBuilding;
        check.evaluation.comparison.verdict = Verdict::Upgrade;
        check.evaluation.comparison.delta = 5;
        report = reportOf(check);
        QCOMPARE(report.verdict, u"Upgrade"_s);
        QCOMPARE(report.score, u"+5%"_s);

        QCOMPARE(reportOn(unique, wearing({{u"Helmet"_s, unique}})).verdict, u"Equipped"_s);
    }

    void gearScores()
    {
        ItemCheck check = judged(Verdict::Upgrade);
        Evaluation &e = check.evaluation;
        e.gear = GearScore{.buildName = u"Minions"_s, .score = 78, .grade = u"A"_s};
        e.gearEquipped = GearScore{.buildName = u"Minions"_s, .score = 56, .grade = u"B"_s};
        ItemReport report = reportOf(check);
        QCOMPARE(report.gearScore, u"78"_s);
        QCOMPARE(report.gearGrade, u"A"_s);
        QCOMPARE(report.yourGearScore, u"56"_s);
        QCOMPARE(report.gearBuild, u"Minions"_s);

        e.gearEquipped->unrated = GearScore::Unrated::Unique;
        report = reportOf(check);
        QCOMPARE(report.gearScore, u"78"_s);
        QCOMPARE(report.yourGearScore, QString());

        e.gear->unrated = GearScore::Unrated::Unidentified;
        QCOMPARE(reportOf(check).gearScore, QString());

        // as Path of Building's is the one that counts
        e.gear->unrated = GearScore::Unrated::No;
        e.mode = ScoreMode::PathOfBuilding;
        report = reportOf(check);
        QCOMPARE(report.gearScore, QString());
        QCOMPARE(report.gearGrade, QString());
    }

    void lossesAndRisks()
    {
        ItemCheck check = judged(Verdict::ResistLoss);
        const QString fire = u"Fire res drops below the cap (60% of 75%)"_s;
        const QString spirit = u"Spirit drops by 10, so check your minions and auras still fit"_s;
        check.evaluation.risks = {fire, spirit};
        check.evaluation.losses = {fire};
        const ItemReport report = reportOf(check);
        QCOMPARE(report.losses, QStringList{fire});
        QCOMPARE(report.risks, QStringList{spirit});
    }

    void caps_data()
    {
        QTest::addColumn<QList<Element>>("capping");
        QTest::addColumn<QString>("caps");

        using enum Element;
        QTest::newRow("none") << QList<Element>{} << QString();
        QTest::newRow("one") << QList{Fire} << u"Caps your fire resistance"_s;
        QTest::newRow("two") << QList{Fire, Cold} << u"Caps your fire and cold resistances"_s;
        QTest::newRow("three") << QList{Fire, Cold, Lightning}
                               << u"Caps your fire, cold and lightning resistances"_s;
    }

    void caps()
    {
        QFETCH(QList<Element>, capping);
        QFETCH(QString, caps);

        PobVerdict v;
        v.verdict = Verdict::Upgrade;
        for (Element e : allElements)
            v.resists << PobResistChange{.element = e, .reachesCap = capping.contains(e)};
        QCOMPARE(reportOf(judgedByPob(v)).caps, caps);
    }

    void headline()
    {
        PobVerdict v;
        v.verdict = Verdict::Upgrade;
        v.damage = {.stat = u"MinionDPS"_s, .from = 100000, .to = 110000, .percent = 10};
        v.ehp = PobChange{.stat = u"TotalEHP"_s, .from = 40000, .to = 39000, .percent = -2.5};
        v.resistGain = 12;
        QCOMPARE(reportOf(judgedByPob(v)).headline,
                 (QList<ReportFigure>{
                     {.label = u"Minion DPS"_s, .change = u"+10%"_s, .tone = Tone::Good},
                     {.label = u"Effective HP"_s, .change = u"-2.5%"_s, .tone = Tone::Bad},
                     {.label = u"Resistance toward the caps"_s, .change = u"+12"_s, .tone = Tone::Good},
                 }));

        // what it doesn't know or doesn't change
        v.damage = {.stat = u"FullDPS"_s, .from = 2000, .to = 2000, .percent = 0};
        v.ehp = PobChange{.stat = u"TotalEHP"_s, .from = 0, .to = 39000};
        v.resistGain = -4;
        QCOMPARE(reportOf(judgedByPob(v)).headline,
                 (QList<ReportFigure>{
                     {.label = u"DPS"_s, .change = u"0%"_s},
                     {.label = u"Resistance toward the caps"_s, .change = u"-4"_s, .tone = Tone::Bad},
                 }));
        QVERIFY(reportOf(judgedByPob({})).headline.isEmpty());
        QVERIFY(reportOf(judged(Verdict::Upgrade)).headline.isEmpty());
    }

    void changesByPob()
    {
        PobVerdict v;
        v.verdict = Verdict::ResistLoss;
        using enum Element;
        v.resists = {
            {.element = Fire, .from = 75, .to = 60, .wasCapped = true, .lost = true},
            {.element = Cold, .from = 50, .to = 75, .capped = true, .reachesCap = true},
            {.element = Lightning, .from = 40, .to = 45.4},
            // over the cap either way
            {.element = Chaos,
             .from = 75,
             .to = 75,
             .totalFrom = 80,
             .totalTo = 90,
             .wasCapped = true,
             .capped = true},
        };
        v.changes = {
            {.stat = u"Life"_s, .from = 1200, .to = 1350},
            {.stat = u"EnergyShield"_s, .from = 2450.4, .to = 2300},
            {.stat = u"SpiritUnreserved"_s, .from = 12, .to = 0},
            {.stat = u"Mana"_s, .from = 80, .to = 95},
        };
        ItemCheck check = judgedByPob(v);
        // which Path of Building knows better
        check.evaluation.change = CharacterChange{.totals = {{u"Life"_s, 1000, 1100, 100}}};
        QCOMPARE(
            reportOf(check).changes,
            (QList<ReportFigure>{
                {.label = u"Fire res"_s,
                 .change = u"-15%"_s,
                 .from = u"75%"_s,
                 .to = u"60%"_s,
                 .note = u"off the cap"_s,
                 .tone = Tone::Bad},
                {.label = u"Cold res"_s,
                 .change = u"+25%"_s,
                 .from = u"50%"_s,
                 .to = u"75%"_s,
                 .note = u"capped"_s,
                 .tone = Tone::Good},
                {.label = u"Lightning res"_s,
                 .change = u"+5%"_s,
                 .from = u"40%"_s,
                 .to = u"45%"_s,
                 .note = u"under the cap"_s,
                 .tone = Tone::Good},
                {.label = u"Life"_s,
                 .change = u"+150"_s,
                 .from = u"1,200"_s,
                 .to = u"1,350"_s,
                 .tone = Tone::Good},
                {.label = u"Energy Shield"_s,
                 .change = u"-150"_s,
                 .from = u"2,450"_s,
                 .to = u"2,300"_s,
                 .tone = Tone::Bad},
                {.label = u"Unreserved Spirit"_s,
                 .change = u"-12"_s,
                 .from = u"12"_s,
                 .to = u"0"_s,
                 .tone = Tone::Bad},
                {.label = u"Mana"_s, .change = u"+15"_s, .from = u"80"_s, .to = u"95"_s, .tone = Tone::Good},
            }));
    }

    void changedCaps()
    {
        PobVerdict v;
        v.verdict = Verdict::Upgrade;
        v.resists = {
            {.element = Element::Fire, .from = 75, .to = 75, .max = 78, .wasCapped = true},
            {.element = Element::Cold, .from = 74, .to = 74, .max = 74, .capped = true},
        };
        QCOMPARE(reportOf(judgedByPob(v)).changes, (QList<ReportFigure>{
                                                       {.label = u"Fire res"_s,
                                                        .change = u"0%"_s,
                                                        .from = u"75%"_s,
                                                        .to = u"75%"_s,
                                                        .note = u"under the cap"_s},
                                                       {.label = u"Cold res"_s,
                                                        .change = u"0%"_s,
                                                        .from = u"74%"_s,
                                                        .to = u"74%"_s,
                                                        .note = u"capped"_s,
                                                        .tone = Tone::Good},
                                                   }));
    }

    void changesByYourGear()
    {
        ItemCheck check = judged(Verdict::Upgrade);
        CharacterChange &change = check.evaluation.change.emplace();
        using enum Element;
        change.resists = {
            {.element = Fire,
             .label = u"Fire res"_s,
             .delta = -20,
             .from = 80,
             .to = 60,
             .safe = 85,
             .under = true},
            {.element = Cold,
             .label = u"Cold res"_s,
             .delta = -5,
             .from = 85,
             .to = 80,
             .safe = 85,
             .thin = true},
            {.element = Lightning,
             .label = u"Lightning res"_s,
             .delta = 10,
             .from = 70,
             .to = 80,
             .safe = 85,
             .wasUnder = true},
            {.element = Chaos, .label = u"Chaos res"_s, .delta = 7, .from = 90, .to = 97, .safe = 85},
        };
        change.totals = {{u"Life"_s, 1200, 1350, 150}, {u"Spirit"_s, 100, 70, -30}};
        change.overall = TotalChange{u"Gear score"_s, 56, 60, 4};
        QCOMPARE(reportOf(check).changes, (QList<ReportFigure>{
                                              {.label = u"Fire res"_s,
                                               .change = u"-20%"_s,
                                               .from = u"80%"_s,
                                               .to = u"60%"_s,
                                               .note = u"under the cap"_s,
                                               .tone = Tone::Bad},
                                              {.label = u"Cold res"_s,
                                               .change = u"-5%"_s,
                                               .from = u"85%"_s,
                                               .to = u"80%"_s,
                                               .note = u"barely capped"_s,
                                               .tone = Tone::Bad},
                                              {.label = u"Lightning res"_s,
                                               .change = u"+10%"_s,
                                               .from = u"70%"_s,
                                               .to = u"80%"_s,
                                               .tone = Tone::Good},
                                              {.label = u"Chaos res"_s,
                                               .change = u"+7%"_s,
                                               .from = u"90%"_s,
                                               .to = u"97%"_s,
                                               .minor = true},
                                              {.label = u"Life"_s,
                                               .change = u"+150"_s,
                                               .from = u"1,200"_s,
                                               .to = u"1,350"_s,
                                               .tone = Tone::Good},
                                              {.label = u"Spirit"_s,
                                               .change = u"-30"_s,
                                               .from = u"100"_s,
                                               .to = u"70"_s,
                                               .tone = Tone::Bad},
                                              {.label = u"Average gear score"_s,
                                               .change = u"+4"_s,
                                               .from = u"56"_s,
                                               .to = u"60"_s,
                                               .tone = Tone::Good},
                                          }));

        change = {};
        change.resists = {
            // without your resistances, only the change to them is known
            {.element = Fire, .label = u"Fire res"_s, .delta = 12},
            {.element = Cold, .label = u"Cold res"_s, .delta = -7, .from = 97, .to = 90, .safe = 85},
        };
        change.overall = TotalChange{u"Gear score"_s, 56, 56, 0};
        QCOMPARE(
            reportOf(check).changes,
            (QList<ReportFigure>{
                {.label = u"Fire res"_s, .change = u"+12%"_s, .tone = Tone::Good},
                {.label = u"Cold res"_s, .change = u"-7%"_s, .from = u"97%"_s, .to = u"90%"_s, .minor = true},
            }));
    }

    void statLines_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<double>("change");
        QTest::addColumn<QString>("line");

        QTest::newRow("more") << u"# to maximum Life"_s << 30.0 << u"+30 to maximum Life"_s;
        QTest::newRow("less") << u"#% to Cold Resistance"_s << -12.0 << u"-12% to Cold Resistance"_s;
        QTest::newRow("a fraction") << u"#% increased Attack Speed"_s << 2.25
                                    << u"+2.3% increased Attack Speed"_s;
        QTest::newRow("thousands") << u"# to Armour"_s << 1234.0 << u"+1,234 to Armour"_s;
        QTest::newRow("a range") << u"Adds # to # Fire Damage to Attacks"_s << 7.5
                                 << u"Adds +7.5 average Fire Damage to Attacks"_s;
        QTest::newRow("a range first")
            << u"# to # Physical Thorns damage"_s << 12.0 << u"+12 average Physical Thorns damage"_s;
        QTest::newRow("two numbers") << u"#% chance to Shock for # seconds"_s << 5.0
                                     << u"+5% chance to Shock for # seconds"_s;
        QTest::newRow("the item's own") << u"[Base] Armour"_s << 120.0 << u"+120 Armour"_s;
        QTest::newRow("a weapon's") << u"[Weapon] Total DPS"_s << -15.5 << u"-15.5 Total DPS"_s;
        QTest::newRow("no number") << u"Onslaught"_s << 1.0 << u"+1 Onslaught"_s;
    }

    void statLines()
    {
        QFETCH(QString, key);
        QFETCH(double, change);
        QFETCH(QString, line);

        ItemCheck check = judged(Verdict::Upgrade);
        check.evaluation.comparison.rows = {{.key = key, .diff = change, .usefulness = 1}};
        QCOMPARE(reportOf(check).stats.value(0).label, line);
    }

    void topStats()
    {
        ItemCheck check = judged(Verdict::Upgrade);
        check.evaluation.comparison.rows = {
            {.key = u"# to maximum Life"_s, .diff = 30, .usefulness = 1},
            {.key = u"#% to Cold Resistance"_s, .diff = -12, .usefulness = 2},
            // more of one you'd rather not have
            {.key = u"#% increased Light Radius"_s, .diff = 15, .usefulness = -0.5},
            {.key = u"# to Accuracy Rating"_s, .diff = 90},
            {.key = u"# to Strength"_s, .diff = 10, .usefulness = 0.5},
            {.key = u"# to Dexterity"_s, .diff = 5, .usefulness = 0.5},
        };
        QCOMPARE(reportOf(check).stats, (QList<ReportFigure>{
                                            {.label = u"+30 to maximum Life"_s, .tone = Tone::Good},
                                            {.label = u"-12% to Cold Resistance"_s, .tone = Tone::Bad},
                                            {.label = u"+15% increased Light Radius"_s, .tone = Tone::Bad},
                                            {.label = u"+10 to Strength"_s, .tone = Tone::Good},
                                        }));
    }

    void pobStatus_data()
    {
        QTest::addColumn<ItemCheck::Pob>("pob");
        QTest::addColumn<bool>("checking");
        QTest::addColumn<QString>("status");

        using enum ItemCheck::Pob;
        QTest::newRow("unused") << Unused << false << QString();
        QTest::newRow("no build")
            << NoBuild << false
            << u"Import this character's build from Path of Building for exact verdicts."_s;
        QTest::newRow("checking") << Checking << true << u"Checking with Path of Building"_s;
        QTest::newRow("checked") << Checked << false << QString();
        QTest::newRow("failed") << Failed << false << u"Path of Building stopped."_s;
    }

    void pobStatus()
    {
        QFETCH(ItemCheck::Pob, pob);
        QFETCH(bool, checking);
        QFETCH(QString, status);

        ItemCheck check = judged(Verdict::Upgrade);
        check.pob = pob;
        if (pob == ItemCheck::Pob::Failed)
            check.pobError = u"Path of Building stopped."_s;
        const ItemReport report = reportOf(check);
        QCOMPARE(report.pobChecking, checking);
        QCOMPARE(report.pobStatus, status);
    }

    void notGear()
    {
        const ItemReport report = reportOn(item(
            u"Item Class: Stackable Currency\nRarity: Currency\nDivine Orb\n--------\nStack Size: 1/20"_s));
        QCOMPARE(report.name, u"Divine Orb"_s);
        QCOMPARE(report.details, QStringList{u"Stackable Currency"_s});
        QVERIFY(!report.gear);
        QCOMPARE(report.wearIn, QStringList());
        QCOMPARE(report.verdict, QString());
        QCOMPARE(report.notes,
                 QStringList{u"Stackable Currency isn't gear, so there's nothing to compare."_s});
    }

    void wearIn()
    {
        QCOMPARE(reportOn(helmetNew).wearIn, QStringList{u"Helmet"_s});
        QCOMPARE(reportOn(helmetNew, wearing({{u"Helmet"_s, helmetOld}})).wearIn, QStringList{u"Helmet"_s});
        QCOMPARE(reportOn(item(itemtexts::advancedRing)).wearIn, (QStringList{u"Ring 1"_s, u"Ring 2"_s}));
        // what you have on already
        QCOMPARE(reportOn(helmetOld, wearing({{u"Helmet"_s, helmetOld}})).wearIn, QStringList());
    }

    void notesOnTheItem()
    {
        const Item corrupted = item(itemtexts::helmetNew + u"\n--------\nCorrupted"_s);
        const ItemReport report = reportOn(corrupted, wearing({{u"Helmet"_s, helmetOld}}));
        QVERIFY(report.notes.contains(u"Corrupted, so it can't be modified any further."_s));
    }
};

QTEST_GUILESS_MAIN(TestItemReport)
#include "tst_itemreport.moc"
