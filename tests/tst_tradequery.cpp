// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/tradequery.h"
#include "tradefixtures.h"

#include <QJsonArray>
#include <QTest>

using namespace pou;

namespace {

TradeData tradeData()
{
    return {StatIndex::fromJson(tradefixtures::stats()), TradeCategories::fromJson(tradefixtures::filters())};
}

QJsonObject queryOf(const TradeQuery &q)
{
    return q.body.value(u"query").toObject();
}

QJsonArray statGroups(const TradeQuery &q)
{
    return queryOf(q).value(u"stats").toArray();
}

QJsonObject filterOf(const TradeQuery &q, QStringView group, QStringView name)
{
    return queryOf(q)
        .value(u"filters")
        .toObject()
        .value(group)
        .toObject()
        .value(u"filters")
        .toObject()
        .value(name)
        .toObject();
}

QJsonObject minimum(double min)
{
    return {{u"min"_s, min}};
}

QJsonObject wanted(const QString &id, const QJsonObject &value)
{
    return {{u"id"_s, id}, {u"value"_s, value}, {u"disabled"_s, false}};
}

QJsonObject weight(double w)
{
    return {{u"weight"_s, w}};
}

Item withStats(const QString &itemClass, const StatMap &stats)
{
    Item item;
    item.itemClass = itemClass;
    item.stats = stats;
    return item;
}

QStringList filterIds(const QJsonObject &group)
{
    QStringList ids;
    for (const QJsonValue &f : group.value(u"filters").toArray())
        ids << f.toObject().value(u"id").toString();
    return ids;
}

StatPick pick(const QString &key, double min)
{
    StatPick p;
    p.key = key;
    p.min = min;
    return p;
}

} // namespace

class TestTradeQuery : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void listingStatus()
    {
        QCOMPARE(listingStatusId(ListingStatus::Available), u"available"_s);
        QCOMPARE(listingStatusId(ListingStatus::Online), u"online"_s);
        QCOMPARE(listingStatusFromId(u"securable"), ListingStatus::Securable);
        QCOMPARE(listingStatusFromId(u"any"), ListingStatus::Any);
        QCOMPARE(listingStatusFromId(u"whatever"), ListingStatus::Available);
    }

    void pickedStats()
    {
        CustomSearchOptions options;
        options.itemClass = u"Helmets"_s;
        const TradeQuery q = customSearch(
            {pick(u"# to maximum Life"_s, 80), pick(u"[Base] Armour"_s, 300), pick(u"made up #"_s, 1)},
            tradeData(), options);
        QCOMPARE(q.unmatched, QStringList{u"made up #"_s});
        QCOMPARE(queryOf(q).value(u"status").toObject().value(u"option").toString(), u"available"_s);
        const QJsonObject first = statGroups(q).at(0).toObject();
        QCOMPARE(first.value(u"type").toString(), u"and"_s);
        QCOMPARE(first.value(u"filters").toArray().at(0).toObject(),
                 wanted(u"pseudo.pseudo_total_life"_s, minimum(80)));
        QCOMPARE(filterOf(q, u"equipment_filters", u"ar"), minimum(300));
        QCOMPARE(filterOf(q, u"type_filters", u"category").value(u"option").toString(), u"armour.helmet"_s);
        QCOMPARE(q.body.value(u"sort").toObject(), QJsonObject({{u"price"_s, u"asc"_s}}));
        QCOMPARE(q.excluded, 0);
    }

    void onlyThisBase()
    {
        CustomSearchOptions options;
        options.itemClass = u"Helmets"_s;
        options.baseType = u"Expert Visored Helm"_s;
        options.matchBase = true;
        options.status = ListingStatus::Online;
        const TradeQuery q = customSearch({}, tradeData(), options);
        QCOMPARE(queryOf(q).value(u"type").toString(), u"Expert Visored Helm"_s);
        QVERIFY(!queryOf(q).value(u"filters").toObject().contains(u"type_filters"));
        QCOMPARE(statGroups(q), QJsonArray());
        QCOMPARE(queryOf(q).value(u"status").toObject().value(u"option").toString(), u"online"_s);
    }

    void handPickedStat()
    {
        StatPick p = pick(u"whatever"_s, 5);
        p.id = u"explicit.stat_ms"_s;
        CustomSearchOptions options;
        options.itemClass = u"Boots"_s;
        const TradeQuery q = customSearch({p}, tradeData(), options);
        QCOMPARE(statGroups(q).at(0).toObject().value(u"filters").toArray().at(0).toObject(),
                 wanted(u"explicit.stat_ms"_s, minimum(5)));
        QCOMPARE(filterOf(q, u"type_filters", u"category").value(u"option").toString(), u"armour.boots"_s);
    }

    void sameKindOfMod()
    {
        using namespace tradefixtures;
        const TradeData data{
            StatIndex::fromJson(response({
                group(u"explicit"_s,
                      {stat(u"explicit.ms"_s, u"#% increased Movement Speed"_s, u"explicit"_s)}),
                group(u"implicit"_s,
                      {stat(u"implicit.ms"_s, u"#% increased Movement Speed"_s, u"implicit"_s)}),
            })),
            {}};
        StatPick p = pick(u"#% increased Movement Speed"_s, 10);
        const auto idOf = [&data](const StatPick &x) {
            const TradeQuery q = customSearch({x}, data, {});
            return statGroups(q)
                .at(0)
                .toObject()
                .value(u"filters")
                .toArray()
                .at(0)
                .toObject()
                .value(u"id")
                .toString();
        };
        QCOMPARE(idOf(p), u"explicit.ms"_s);
        p.origin = StatOrigin::Implicit;
        QCOMPARE(idOf(p), u"implicit.ms"_s);
        // no augment version, so the explicit one
        p.origin = StatOrigin::Socket;
        QCOMPARE(idOf(p), u"explicit.ms"_s);
    }

    void excludedStats()
    {
        CustomSearchOptions options;
        options.itemClass = u"Helmets"_s;
        options.ignoreRules = {{u"res"_s, {}, IgnoreRule::Mode::Exclude}};
        const TradeQuery q = customSearch({pick(u"# to maximum Life"_s, 10)}, tradeData(), options);
        const QJsonObject excluded = statGroups(q).at(1).toObject();
        QCOMPARE(excluded.value(u"type").toString(), u"not"_s);
        // only fire is in the test stat list
        QCOMPARE(excluded.value(u"filters").toArray(),
                 QJsonArray({QJsonObject{{u"id"_s, u"pseudo.pseudo_total_fire_resistance"_s},
                                         {u"disabled"_s, false}}}));
        QCOMPARE(q.excluded, 1);

        // plain ignore rules don't leave anything out of the search
        options.ignoreRules = {{u"res"_s, {}, IgnoreRule::Mode::Ignore}};
        QCOMPARE(statGroups(customSearch({pick(u"# to maximum Life"_s, 10)}, tradeData(), options)).size(),
                 1);
    }

    void weightedSum()
    {
        const Item helmet = withStats(
            u"Helmets"_s,
            {{u"# to maximum Life"_s, 50}, {u"#% to Fire Resistance"_s, 20}, {u"[Base] Armour"_s, 300}});
        const Weights weights = {{u"# to maximum Life"_s, 1},
                                 {u"#% to Fire Resistance"_s, 1.5},
                                 {u"[Base] Armour"_s, 0.05},
                                 {u"unknown stat #"_s, 2}};
        const WeightedSearch q = weightedSearch(helmet, weights, tradeData());
        QCOMPARE(q.current, 80.0);
        QCOMPARE(q.groupMin, 81.0);
        const QJsonObject group = statGroups(q).at(0).toObject();
        QCOMPARE(group.value(u"type").toString(), u"weight"_s);
        QCOMPARE(group.value(u"value").toObject(), minimum(81));
        QCOMPARE(group.value(u"filters").toArray(),
                 QJsonArray({wanted(u"pseudo.pseudo_total_life"_s, weight(1)),
                             wanted(u"pseudo.pseudo_total_fire_resistance"_s, weight(1.5))}));
        // base defences depend on the base type, so they're left to you
        QVERIFY(!queryOf(q).value(u"filters").toObject().contains(u"equipment_filters"));
        QCOMPARE(filterOf(q, u"type_filters", u"category").value(u"option").toString(), u"armour.helmet"_s);
        QVERIFY(q.unmatched.isEmpty());
        QCOMPARE(q.body.value(u"sort").toObject(), QJsonObject({{u"statgroup.0"_s, u"desc"_s}}));

        WeightedSearchOptions better;
        better.minGainPercent = 10;
        QCOMPARE(weightedSearch(helmet, weights, tradeData(), better).groupMin, 89.0);
    }

    void weaponDpsAsMinimum()
    {
        const Item mace = withStats(u"Two Hand Maces"_s, {{u"[Weapon] Total DPS"_s, 176.4}});
        const WeightedSearch q = weightedSearch(mace, {{u"[Weapon] Total DPS"_s, 1}}, tradeData());
        QCOMPARE(filterOf(q, u"equipment_filters", u"dps"), minimum(176));
        QCOMPARE(filterOf(q, u"type_filters", u"category").value(u"option").toString(), u"weapon.twomace"_s);
        QCOMPARE(statGroups(q), QJsonArray());
    }

    void runesDontCount()
    {
        const Item helmet = parseItem(u"Item Class: Helmets\nRarity: Rare\nA\nB\n--------\n"
                                      "+12% to Fire Resistance (rune)\n--------\n+20% to Fire Resistance"_s)
                                .value();
        const WeightedSearch q = weightedSearch(helmet, {{u"#% to Fire Resistance"_s, 1}}, tradeData());
        QCOMPARE(q.current, 20.0);
        QCOMPARE(q.groupMin, 21.0);
    }

    void weightedSearchSkipsIgnoredStats()
    {
        const Item helmet = withStats(
            u"Helmets"_s,
            {{u"# to maximum Life"_s, 50}, {u"#% to Fire Resistance"_s, 30}, {u"# to Spirit"_s, 20}});
        WeightedSearchOptions options;
        options.ignoreRules = {{u"res"_s, {}, IgnoreRule::Mode::Ignore}};
        const WeightedSearch q = weightedSearch(
            helmet, {{u"# to maximum Life"_s, 1}, {u"#% to Fire Resistance"_s, 2}, {u"# to Spirit"_s, 1}},
            tradeData(), options);
        QCOMPARE(filterIds(statGroups(q).at(0).toObject()), QStringList{u"pseudo.pseudo_total_life"_s});
        QCOMPARE(q.current, 50.0);
        // not in the trade site's list, and your item has it
        QCOMPARE(q.unmatched, QStringList{u"# to Spirit"_s});
        QCOMPARE(q.excluded, 0);
    }
};

QTEST_GUILESS_MAIN(TestTradeQuery)
#include "tst_tradequery.moc"
