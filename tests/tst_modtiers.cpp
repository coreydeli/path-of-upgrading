// SPDX-License-Identifier: MIT
#include "core/modtiers.h"

#include <QJsonDocument>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString boots = u"Boots"_s;
const QString life = u"# to maximum Life"_s;

SlotMods table()
{
    const auto json = QJsonDocument::fromJson("{\"classes\":{\"Boots\":{"
                                              "\"# to maximum Life\":[[60,120,149],[30,70,119],[1,10,69]],"
                                              "\"#% increased Rarity of Items found\":[[30,15,25],[1,10,20]],"
                                              "\"# to Spirit\":[[50,10,20,1]],"
                                              "\"# to Strength\":[[50,30,40],[20,10,29],[40,35,45,1]]}}}");
    return SlotMods::fromJson(json.object());
}

} // namespace

class TestModTiers : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theTopTierIsTierOne()
    {
        const auto tier = tierOf(table(), boots, life, 130);
        QVERIFY(tier);
        QCOMPARE(tier->number, 1);
        QCOMPARE(tier->count, 3);
        QVERIFY(!tier->better);
        QVERIFY(nextTierText(*tier).isEmpty());
    }

    void aLowerTierSaysWhatIsNext()
    {
        const auto tier = tierOf(table(), boots, life, 100);
        QVERIFY(tier);
        QCOMPARE(tier->number, 2);
        QCOMPARE(tierText(*tier), u"tier 2 of 3"_s);
        QCOMPARE(tier->better->level, 60);
        QCOMPARE(nextTierText(*tier), u"tier 1 (120 to 149) from item level 60"_s);
    }

    void theEdgesBelongToTheTier()
    {
        QCOMPARE(tierOf(table(), boots, life, 120)->number, 1);
        QCOMPARE(tierOf(table(), boots, life, 149)->number, 1);
        QCOMPARE(tierOf(table(), boots, life, 70)->number, 2);
        QCOMPARE(tierOf(table(), boots, life, 10)->number, 3);
        QCOMPARE(tierOf(table(), boots, life, 69)->number, 3);
    }

    void nothingAboveTheTopOrBelowTheBottom()
    {
        QVERIFY(!tierOf(table(), boots, life, 150));
        QVERIFY(!tierOf(table(), boots, life, 9));
    }

    void nothingBetweenTwoTiers()
    {
        const auto json = QJsonDocument::fromJson(
            "{\"classes\":{\"Boots\":{\"# to maximum Life\":[[60,120,149],[1,10,69]]}}}");
        QVERIFY(!tierOf(SlotMods::fromJson(json.object()), boots, life, 100));
    }

    void nothingForWhatTheTableLacks()
    {
        QVERIFY(!tierOf(table(), u"Gloves"_s, life, 100));
        QVERIFY(!tierOf(table(), boots, u"# to Dexterity"_s, 10));
        QVERIFY(!tierOf(SlotMods(), boots, life, 100));
    }

    void nothingWhenTiersOverlap()
    {
        const QString rarity = u"#% increased Rarity of Items found"_s;
        QVERIFY(!tierOf(table(), boots, rarity, 18));
        QCOMPARE(tierOf(table(), boots, rarity, 24)->number, 1);
        QCOMPARE(tierOf(table(), boots, rarity, 12)->number, 2);
    }

    void nothingWhenAHybridCouldHaveGivenIt()
    {
        const QString strength = u"# to Strength"_s;
        QVERIFY(!tierOf(table(), boots, strength, 36));
        QCOMPARE(tierOf(table(), boots, strength, 31)->number, 1);
        // a stat with only hybrid tiers has no tier of its own
        QVERIFY(!tierOf(table(), boots, u"# to Spirit"_s, 15));
    }

    void whatTierAMinimumNeeds()
    {
        QCOMPARE(tierFor(table(), boots, life, 120), 1);
        QCOMPARE(tierFor(table(), boots, life, 121), 1);
        QCOMPARE(tierFor(table(), boots, life, 119), 2);
        QCOMPARE(tierFor(table(), boots, life, 5), 3);
        QVERIFY(!tierFor(table(), boots, life, 150));
        QVERIFY(!tierFor(table(), u"Gloves"_s, life, 5));
    }
};

QTEST_GUILESS_MAIN(TestModTiers)
#include "tst_modtiers.moc"
