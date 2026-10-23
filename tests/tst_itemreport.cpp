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
        QCOMPARE(report.details, (QStringList{u"Expert Visored Helm"_s, u"Helmets"_s, u"Item level 79"_s}));
    }

    void namedAfterItsBase()
    {
        const ItemReport report =
            reportOn(item(u"Item Class: Rings\nRarity: Normal\nGold Ring\n--------\n+10 to maximum Life"_s));
        QCOMPARE(report.name, u"Gold Ring"_s);
        QCOMPARE(report.details, QStringList{u"Rings"_s});
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
        QCOMPARE(report.verdict, QString());
        QCOMPARE(report.notes,
                 QStringList{u"Stackable Currency isn't gear, so there's nothing to compare."_s});
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
