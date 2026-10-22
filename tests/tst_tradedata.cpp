// SPDX-License-Identifier: MIT
#include "core/tradedata.h"
#include "tradefixtures.h"

#include <QTest>

using namespace pou;

class TestTradeData : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pseudoStatsFirst()
    {
        const StatIndex index = StatIndex::fromJson(tradefixtures::stats());
        QCOMPARE(index.all().size(), 7);
        QCOMPARE(index.match(u"# to maximum Life"_s)->id, u"pseudo.pseudo_total_life"_s);
        QCOMPARE(index.match(u"#% to Fire Resistance"_s)->id, u"pseudo.pseudo_total_fire_resistance"_s);
        QCOMPARE(index.match(u"#% increased Movement Speed"_s)->id, u"explicit.stat_ms"_s);
        QVERIFY(!index.match(u"nonsense #"_s));
    }

    void localAndGlobalMods()
    {
        const StatIndex index = StatIndex::fromJson(tradefixtures::stats());
        QCOMPARE(index.match(u"#% increased Armour"_s, {.preferLocal = true})->id,
                 u"explicit.stat_armour_local"_s);
        QCOMPARE(index.match(u"#% increased Armour"_s, {.preferLocal = false})->id,
                 u"explicit.stat_armour"_s);
    }

    void onlyOneType()
    {
        const StatIndex index = StatIndex::fromJson(tradefixtures::stats());
        // no implicit life in the list, so the explicit one rather than the pseudo
        const auto life = index.match(u"# to maximum Life"_s, {.type = u"implicit"_s});
        QCOMPARE(life->id, u"explicit.stat_life"_s);
        QCOMPARE(life->type, u"explicit"_s);
        QVERIFY(!index.match(u"nonsense #"_s, {.type = u"implicit"_s}));
    }

    void equipmentFilters()
    {
        const StatIndex index;
        const auto armour = index.match(u"[Base] Armour"_s);
        QCOMPARE(armour->kind, StatMatch::Kind::Equipment);
        QCOMPARE(armour->id, u"ar"_s);
        QCOMPARE(equipmentFilter(u"[Weapon] Total DPS"), u"dps"_s);
        QVERIFY(equipmentFilter(u"[Weapon] Chaos DPS").isEmpty());
        QCOMPARE(equipmentKeys().first(), u"[Base] Armour"_s);
    }

    void categories()
    {
        const TradeCategories fetched = TradeCategories::fromJson(tradefixtures::filters());
        QCOMPARE(fetched.categoryFor(u"Helmets"_s), u"armour.helmet"_s);
        QCOMPARE(fetched.categoryFor(u"Two Hand Maces"_s), u"weapon.twomace"_s);
        QCOMPARE(fetched.categoryFor(u"Quarterstaves"_s), u"weapon.warstaff"_s);
        QCOMPARE(fetched.categoryFor(u"Boots"_s), u"armour.boots"_s);
        QCOMPARE(TradeCategories().categoryFor(u"Boots"_s), u"armour.boots"_s);
        QVERIFY(TradeCategories().categoryFor(u"Stackable Currency"_s).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestTradeData)
#include "tst_tradedata.moc"
