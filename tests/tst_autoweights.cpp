// SPDX-License-Identifier: MIT
#include "core/autoweights.h"
#include "core/parser.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

// a minion build's energy shield gear, one item with a rune
Equipped minionGear()
{
    return {
        {u"Helmet"_s,
         item(u"Item Class: Helmets\nRarity: Rare\nA\nB\n--------\nEnergy Shield: 400\n--------\n"
              "+2 to Level of all Minion Skills\n+30 to Intelligence\n"
              "+10% to Fire Resistance (rune)"_s)},
        {u"Gloves"_s, item(u"Item Class: Gloves\nRarity: Rare\nC\nD\n--------\nEnergy Shield: 200\n--------\n"
                           "+30 to Intelligence\n+40 to maximum Life"_s)},
    };
}

const SuggestedWeight *find(const QList<SuggestedWeight> &suggestions, QStringView key)
{
    for (const SuggestedWeight &s : suggestions) {
        if (s.key == key)
            return &s;
    }
    return nullptr;
}

std::optional<double> weightOf(const QList<SuggestedWeight> &suggestions, QStringView key)
{
    if (const SuggestedWeight *s = find(suggestions, key))
        return s->weight;
    return std::nullopt;
}

} // namespace

class TestAutoWeights : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stackedStats()
    {
        const QList<SuggestedWeight> s = suggestWeights(minionGear());
        QCOMPARE(weightOf(s, u"# to Intelligence"), 1.5); // on both: 45 points for 30
        QCOMPARE(weightOf(s, u"# to maximum Life"), 0.75);
        QCOMPARE(weightOf(s, u"# to Level of all Minion Skills"), 15.0);
        QCOMPARE(weightOf(s, u"[Base] Energy Shield"), 0.13); // 40 points for 300
        // only on a rune
        QVERIFY(!weightOf(s, u"#% to Fire Resistance"));

        const SuggestedWeight *intelligence = find(s, u"# to Intelligence");
        QVERIFY(intelligence);
        QCOMPARE(intelligence->items, 2);
        QCOMPARE(intelligence->average, 30.0);
    }

    void onMostItemsFirst()
    {
        const QList<SuggestedWeight> s = suggestWeights(minionGear());
        QCOMPARE(s.size(), 4);
        QCOMPARE(s.at(0).items, 2);
        QCOMPARE(s.at(1).items, 2);
        QCOMPARE(s.at(2).items, 1);
        // then by name
        QCOMPARE(s.at(2).key, u"# to Level of all Minion Skills"_s);
        QCOMPARE(s.at(3).key, u"# to maximum Life"_s);
    }

    void ignoredStats()
    {
        const QList<SuggestedWeight> s =
            suggestWeights(minionGear(), [](const QString &key) { return key == u"# to maximum Life"; });
        QVERIFY(!find(s, u"# to maximum Life"));
        QVERIFY(find(s, u"# to Intelligence"));
    }

    void leavesOutUniquesAndLocalMods()
    {
        const Equipped gear = {
            {u"Body Armour"_s, item(u"Item Class: Body Armours\nRarity: Unique\nA\nB\n--------\n"
                                    "+100 to maximum Life"_s)},
            {u"Gloves"_s,
             item(u"Item Class: Gloves\nRarity: Rare\nC\nD\n--------\nEnergy Shield: 200\n--------\n"
                  "80% increased Energy Shield\n+30 to maximum Energy Shield"_s)},
            {u"Amulet"_s, item(u"Item Class: Amulets\nRarity: Rare\nE\nF\n--------\n"
                               "+30 to maximum Energy Shield\n25% increased Critical Hit Chance"_s)},
        };
        const QList<SuggestedWeight> s = suggestWeights(gear);
        QVERIFY(!find(s, u"# to maximum Life"));
        QVERIFY(!find(s, u"#% increased Energy Shield"));
        // the gloves' is in their base, the amulet's isn't
        const SuggestedWeight *energyShield = find(s, u"# to maximum Energy Shield");
        QVERIFY(energyShield);
        QCOMPARE(energyShield->items, 1);
        QCOMPARE(weightOf(s, u"#% increased Critical Hit Chance"), 1.2);
    }

    void weaponsAndBoots()
    {
        const Equipped gear = {
            {u"Weapon"_s, item(u"Item Class: One Hand Maces\nRarity: Rare\nA\nB\n--------\n"
                               "Physical Damage: 10-20\nCritical Hit Chance: 5.00%\n"
                               "Attacks per Second: 1.50\n--------\n+50 to Accuracy Rating"_s)},
            {u"Boots"_s, item(u"Item Class: Boots\nRarity: Rare\nC\nD\n--------\n"
                              "25% increased Movement Speed"_s)},
        };
        const QList<SuggestedWeight> s = suggestWeights(gear);
        QCOMPARE(weightOf(s, u"[Weapon] Total DPS"), 2.7); // 60 points for 22.5
        QCOMPARE(weightOf(s, u"#% increased Movement Speed"), 3.6);
        QCOMPARE(weightOf(s, u"# to Accuracy Rating"), 0.6);
        // part of the DPS
        QVERIFY(!find(s, u"[Weapon] Physical DPS"));
        QVERIFY(!find(s, u"[Weapon] Attacks per Second"));
        QVERIFY(!find(s, u"[Weapon] Critical Hit Chance"));
    }

    void nothingWorn() { QVERIFY(suggestWeights({}).isEmpty()); }
};

QTEST_GUILESS_MAIN(TestAutoWeights)
#include "tst_autoweights.moc"
