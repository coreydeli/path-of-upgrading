// SPDX-License-Identifier: MIT
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
};

QTEST_GUILESS_MAIN(TestTradeQuery)
#include "tst_tradequery.moc"
