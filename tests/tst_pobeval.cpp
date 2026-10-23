// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/pobeval.h"

#include <QJsonDocument>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

QJsonObject json(const char *text)
{
    return QJsonDocument::fromJson(text).object();
}

const PobStats before = {{u"FullDPS"_s, 1000}, {u"TotalEHP"_s, 5000}};

PobPlacement ring(const QString &slot, const QString &replacing, double dps)
{
    return {.slot = slot,
            .label = slot,
            .replacing = replacing,
            .stats = {{u"FullDPS"_s, dps}, {u"TotalEHP"_s, 5000}}};
}

const Item grimGrasp =
    parseItem(u"Item Class: Rings\nRarity: Rare\nGrim Grasp\nPrismatic Ring\n--------\n+40 to maximum Life"_s)
        .value();

} // namespace

class TestPobEval : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsAnEval()
    {
        // a ring tried in both ring slots, and an item asked about a slot there's none of
        const PobEval eval = readPobEval(json(R"({
            "id": 7, "ok": true,
            "before": {"Life": 1500, "FireResist": 75, "ChaosInoculation": 0},
            "results": [
                {"ok": true, "slots": [
                    {"slot": "Ring 1", "label": "Ring 1", "replacing": "Grim Grasp, Prismatic Ring",
                     "stats": {"Life": 1548, "FireResist": 70}},
                    {"slot": "Ring 2", "label": "Ring 2", "stats": {"Life": 1641}}
                ]},
                {"ok": false, "error": "Path of Building has no slot called Elbow."}
            ]
        })"));

        QCOMPARE(eval.before,
                 (PobStats{{u"Life"_s, 1500}, {u"FireResist"_s, 75}, {u"ChaosInoculation"_s, 0}}));
        QCOMPARE(eval.items.size(), 2);

        const PobItemEval &ring = eval.items.at(0);
        QCOMPARE(ring.error, QString());
        QCOMPARE(ring.placements.size(), 2);
        QCOMPARE(ring.placements.at(0).slot, u"Ring 1"_s);
        QCOMPARE(ring.placements.at(0).label, u"Ring 1"_s);
        QCOMPARE(ring.placements.at(0).replacing, u"Grim Grasp, Prismatic Ring"_s);
        QCOMPARE(ring.placements.at(0).stats, (PobStats{{u"Life"_s, 1548}, {u"FireResist"_s, 70}}));
        // with nothing in it
        QCOMPARE(ring.placements.at(1).slot, u"Ring 2"_s);
        QCOMPARE(ring.placements.at(1).replacing, QString());
        QCOMPARE(ring.placements.at(1).stats, (PobStats{{u"Life"_s, 1641}}));

        const PobItemEval &nowhere = eval.items.at(1);
        QCOMPARE(nowhere.error, u"Path of Building has no slot called Elbow."_s);
        QVERIFY(nowhere.placements.isEmpty());
    }

    void onlyNumbers()
    {
        const PobStats stats = readPobStats(json(R"({"Life": 61, "Name": "Doom Vise", "Mana": null})"));
        QCOMPARE(stats, (PobStats{{u"Life"_s, 61}}));
    }

    void errorWithoutWords()
    {
        const PobEval eval = readPobEval(json(R"({"ok": true, "before": {}, "results": [{"ok": false}]})"));
        QCOMPARE(eval.items.size(), 1);
        QCOMPARE(eval.items.at(0).error, u"Path of Building couldn't try this item."_s);
    }

    void itemNames_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("name");

        QTest::newRow("rare") << u"Item Class: Rings\nRarity: Rare\nGrim Grasp\nPrismatic Ring\n--------\n"
                                 "+40 to maximum Life"_s
                              << u"Grim Grasp, Prismatic Ring"_s;
        QTest::newRow("unique") << u"Item Class: Helmets\nRarity: Unique\nMournful Visage\nVisored Helm\n"
                                   "--------\nArmour: 100"_s
                                << u"Mournful Visage, Visored Helm"_s;
        QTest::newRow("magic") << u"Item Class: Rings\nRarity: Magic\nHale Gold Ring of the Fox\n--------\n"
                                  "+20 to maximum Life"_s
                               << u"Hale Gold Ring of the Fox"_s;
        QTest::newRow("normal") << u"Item Class: Helmets\nRarity: Normal\nSuperior Visored Helm\n--------\n"
                                   "Armour: 100"_s
                                << u"Superior Visored Helm"_s;
    }

    void itemNames()
    {
        QFETCH(QString, text);
        QFETCH(QString, name);
        QCOMPARE(pobItemName(parseItem(text).value()), name);
    }

    void bestPlace()
    {
        const PobItemEval eval = {.placements = {
                                      ring(u"Ring 1"_s, u"Doom Loop, Gold Ring"_s, 1100),
                                      ring(u"Ring 2"_s, u"Woe Band, Ruby Ring"_s, 1300),
                                  }};
        const auto judgement = bestPlacement(before, eval, grimGrasp);
        QVERIFY(judgement);
        QCOMPARE(judgement->placement.slot, u"Ring 2"_s);
        QCOMPARE(judgement->verdict.verdict, Verdict::Upgrade);
        QCOMPARE(judgement->verdict.score, judgeWithPob(before, eval.placements.at(1).stats).score);
    }

    void itemYouWear()
    {
        // it can't be in the other ring slot as well
        const PobItemEval eval = {.placements = {
                                      ring(u"Ring 1"_s, u"Doom Loop, Gold Ring"_s, 1300),
                                      ring(u"Ring 2"_s, u"Grim Grasp, Prismatic Ring"_s, 1000),
                                  }};
        const auto judgement = bestPlacement(before, eval, grimGrasp);
        QVERIFY(judgement);
        QCOMPARE(judgement->placement.slot, u"Ring 2"_s);
        QCOMPARE(judgement->verdict.verdict, Verdict::Same);
    }

    void itemWithoutAName()
    {
        // which isn't the one in an empty slot
        const PobItemEval eval = {.placements = {
                                      ring(u"Ring 1"_s, QString(), 1100),
                                      ring(u"Ring 2"_s, u"Doom Loop, Gold Ring"_s, 1300),
                                  }};
        const auto judgement = bestPlacement(before, eval, Item{});
        QVERIFY(judgement);
        QCOMPARE(judgement->placement.slot, u"Ring 2"_s);
    }

    void triedNowhere()
    {
        const PobItemEval eval = {.error = u"There is nowhere in the build this item can go."_s};
        QVERIFY(!bestPlacement(before, eval, grimGrasp));
    }
};

QTEST_GUILESS_MAIN(TestPobEval)
#include "tst_pobeval.moc"
