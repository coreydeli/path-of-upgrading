// SPDX-License-Identifier: MIT
#include "core/tradedata.h"
#include "tradefixtures.h"

#include <QTest>

using namespace pou;

namespace {

// Shaped like the category filter in /api/trade2/data/filters, with these options.
QJsonObject categoriesOf(const QJsonArray &options)
{
    const QJsonObject category = {{u"id"_s, u"category"_s},
                                  {u"option"_s, QJsonObject{{u"options"_s, options}}}};
    return tradefixtures::response(
        {QJsonObject{{u"id"_s, u"type_filters"_s}, {u"filters"_s, QJsonArray{category}}}});
}

} // namespace

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

    void entriesNeedAnIdAndText()
    {
        using namespace tradefixtures;
        const StatIndex index = StatIndex::fromJson(response(
            {group(u"explicit"_s, {
                                      stat(QString(), u"+# to Spirit"_s, u"explicit"_s),
                                      stat(u"explicit.stat_mana"_s, QString(), u"explicit"_s),
                                      stat(u"explicit.stat_life"_s, u"+# to maximum Life"_s, u"explicit"_s),
                                  })}));
        QCOMPARE(index.all().size(), 1);
        QCOMPARE(index.all().first().id, u"explicit.stat_life"_s);
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

    void fetchedCategoriesComeFirst()
    {
        using namespace tradefixtures;
        const TradeCategories fetched = TradeCategories::fromJson(categoriesOf({
            categoryOption(QJsonValue::Null, u"Helmet"_s),
            categoryOption(u"armour.helmet_new"_s, u"Helmet"_s),
            categoryOption(u"armour.helmet_again"_s, u"Helmet"_s),
            categoryOption(u"armour.focus_new"_s, u"Focus"_s),
            categoryOption(u"weapon.staff_new"_s, u"Staff"_s),
        }));
        // over the built-in ones, and the first of a name
        QCOMPARE(fetched.categoryFor(u"Helmets"_s), u"armour.helmet_new"_s);
        QCOMPARE(fetched.categoryFor(u"Foci"_s), u"armour.focus_new"_s);
        QCOMPARE(fetched.categoryFor(u"Staves"_s), u"weapon.staff_new"_s);
        // and the built-in ones for the rest
        QCOMPARE(fetched.categoryFor(u"Gloves"_s), u"armour.gloves"_s);

        const TradeCategories none = TradeCategories::fromJson(categoriesOf({
            categoryOption(QJsonValue::Null, u"Any"_s),
        }));
        QVERIFY(none.isEmpty());
    }

    void categories()
    {
        const TradeCategories fetched = TradeCategories::fromJson(tradefixtures::filters());
        QVERIFY(!fetched.isEmpty());
        QCOMPARE(fetched.categoryFor(u"Helmets"_s), u"armour.helmet"_s);
        QCOMPARE(fetched.categoryFor(u"Two Hand Maces"_s), u"weapon.twomace"_s);
        QCOMPARE(fetched.categoryFor(u"Quarterstaves"_s), u"weapon.warstaff"_s);
        QCOMPARE(fetched.categoryFor(u"Boots"_s), u"armour.boots"_s);
        QVERIFY(TradeCategories().isEmpty());
        QCOMPARE(TradeCategories().categoryFor(u"Boots"_s), u"armour.boots"_s);
        QVERIFY(TradeCategories().categoryFor(u"Stackable Currency"_s).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestTradeData)
#include "tst_tradedata.moc"
