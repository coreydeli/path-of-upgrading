// SPDX-License-Identifier: MIT
#include "core/pobweighing.h"

#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

QJsonObject mod(const QJsonValue &id, const QJsonValue &weight)
{
    return {{u"id"_s, id}, {u"weight"_s, weight}};
}

} // namespace

class TestPobWeighing : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void damageAndEhp()
    {
        const PobStats build = {{u"FullDPS"_s, 12000}, {u"CombinedDPS"_s, 9000}, {u"TotalEHP"_s, 6000}};
        const QList<PobStatWeight> stats = pobSearchStats(build, 0.6);
        QCOMPARE(stats.size(), 2);
        QCOMPARE(stats[0].stat, u"FullDPS"_s);
        QCOMPARE(stats[0].weight, 0.6);
        QCOMPARE(stats[1].stat, u"TotalEHP"_s);
        QCOMPARE(stats[1].weight, 0.4);
    }

    void theDamageItsJudgedBy()
    {
        // which Path of Building keeps under Minion
        QCOMPARE(pobSearchStats({{u"MinionDPS"_s, 3000}}, 0.5)[0].stat, u"MinionCombinedDPS"_s);
        QCOMPARE(pobSearchStats({{u"FullDPS"_s, 0}, {u"CombinedDPS"_s, 800}}, 0.5)[0].stat, u"CombinedDPS"_s);
        QCOMPARE(pobSearchStats({}, 0.5)[0].stat, u"TotalDPS"_s);
    }

    void effectiveHpAlwaysCounts()
    {
        const PobStats build = {{u"FullDPS"_s, 12000}};
        QCOMPARE(pobSearchStats(build, 1), (QList<PobStatWeight>{{u"FullDPS"_s, 1}, {u"TotalEHP"_s, 0.05}}));
        QCOMPARE(pobSearchStats(build, 3), (QList<PobStatWeight>{{u"FullDPS"_s, 1}, {u"TotalEHP"_s, 0.05}}));
        QCOMPARE(pobSearchStats(build, -1), (QList<PobStatWeight>{{u"FullDPS"_s, 0}, {u"TotalEHP"_s, 1}}));
    }

    void readsTheAnswer()
    {
        const QJsonObject answer = {
            {u"ok"_s, true},
            {u"category"_s, u"armour.gloves"_s},
            {u"min"_s, 140.08},
            {u"mods"_s, QJsonArray{mod(u"explicit.stat_life"_s, 2.5), mod(u"explicit.stat_fire"_s, 0.8)}},
        };
        const PobWeighing weighing = readPobWeighing(answer);
        QCOMPARE(weighing.category, u"armour.gloves"_s);
        QCOMPARE(weighing.min, 140.08);
        QCOMPARE(weighing.mods.size(), 2);
        QCOMPARE(weighing.mods[0].id, u"explicit.stat_life"_s);
        QCOMPARE(weighing.mods[0].weight, 2.5);
        QCOMPARE(weighing.mods[1].id, u"explicit.stat_fire"_s);
        QCOMPARE(weighing.mods[1].weight, 0.8);
    }

    void leavesOutWhatCantBeSearched()
    {
        const QJsonObject answer = {
            {u"min"_s, -3},
            {u"mods"_s, QJsonArray{mod(QJsonValue(), 2), mod(u""_s, 2), mod(u"explicit.stat_none"_s, 0),
                                   mod(u"explicit.stat_less"_s, -1), mod(u"explicit.stat_text"_s, u"2"_s),
                                   mod(u"explicit.stat_life"_s, 1)}},
        };
        const PobWeighing weighing = readPobWeighing(answer);
        QCOMPARE(weighing.category, QString());
        QCOMPARE(weighing.min, 0.0);
        QCOMPARE(weighing.mods.size(), 1);
        QCOMPARE(weighing.mods[0].id, u"explicit.stat_life"_s);

        QCOMPARE(readPobWeighing({}).mods.size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestPobWeighing)
#include "tst_pobweighing.moc"
