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

// What the group's filter for the stat asks for.
QJsonObject valueOf(const QJsonObject &group, const QString &id)
{
    for (const QJsonValue &f : group.value(u"filters").toArray()) {
        if (f.toObject().value(u"id").toString() == id)
            return f.toObject().value(u"value").toObject();
    }
    return {};
}

QJsonObject firstValue(const TradeQuery &q)
{
    const QJsonArray filters = statGroups(q).at(0).toObject().value(u"filters").toArray();
    return filters.at(0).toObject().value(u"value").toObject();
}

int filterCount(const TradeQuery &q)
{
    int n = 0;
    for (const QJsonValue &group : statGroups(q))
        n += int(group.toObject().value(u"filters").toArray().size());
    return n;
}

Item jewel()
{
    return parseItem(u"Item Class: Jewels\nRarity: Rare\nHonour Stone\nSapphire\n--------\nItem Level: 80\n"
                     "--------\n12% increased maximum Energy Shield\nMinions deal 14% increased Damage\n"
                     "+8 to Intelligence"_s)
        .value();
}

StatPick pick(const QString &key, double min)
{
    StatPick p;
    p.key = key;
    p.min = min;
    return p;
}

// A site with an explicit "+# to <name>" for each name.
TradeData tradeDataFor(const QStringList &names)
{
    using namespace tradefixtures;
    QJsonArray entries;
    for (const QString &name : names)
        entries << stat(u"explicit."_s + name.toLower(), u"+# to "_s + name, u"explicit"_s);
    return {StatIndex::fromJson(response({group(u"explicit"_s, entries)})), {}};
}

// A site with damage, defence and resistance mods, for Path of Building's
// weights.
TradeData pobSite()
{
    using namespace tradefixtures;
    return {
        StatIndex::fromJson(response({
            group(u"explicit"_s,
                  {
                      stat(u"explicit.speed"_s, u"#% increased Attack Speed"_s, u"explicit"_s),
                      stat(u"explicit.phys"_s, u"Adds # to # Physical Damage to Attacks"_s, u"explicit"_s),
                      stat(u"explicit.crit"_s, u"#% increased Critical Hit Chance for Attacks"_s,
                           u"explicit"_s),
                      stat(u"explicit.life"_s, u"+# to maximum Life"_s, u"explicit"_s),
                      stat(u"explicit.recoup"_s, u"#% of Damage taken Recouped as Life"_s, u"explicit"_s),
                      stat(u"explicit.fire"_s, u"+#% to Fire Resistance"_s, u"explicit"_s),
                      stat(u"explicit.cold"_s, u"+#% to Cold Resistance"_s, u"explicit"_s),
                      stat(u"explicit.chaos"_s, u"+#% to Chaos Resistance"_s, u"explicit"_s),
                      stat(u"explicit.fire_cold"_s, u"+#% to Fire and Cold Resistances"_s, u"explicit"_s),
                      stat(u"explicit.all_res"_s, u"+#% to all Elemental Resistances"_s, u"explicit"_s),
                      stat(u"explicit.ally_res"_s,
                           u"Allies in your Presence have +#% to all Elemental Resistances"_s, u"explicit"_s),
                  }),
            group(u"implicit"_s, {stat(u"implicit.fire"_s, u"+#% to Fire Resistance"_s, u"implicit"_s)}),
            group(u"rune"_s, {stat(u"rune.fire"_s, u"+#% to Fire Resistance"_s, u"rune"_s)}),
            group(u"pseudo"_s, {stat(u"pseudo.fire"_s, u"+#% total to Fire Resistance"_s, u"pseudo"_s)}),
        })),
        TradeCategories::fromJson(tradefixtures::filters())};
}

// The "equipment_filters" ids of a search.
QStringList equipmentIds(const TradeQuery &q)
{
    return queryOf(q)
        .value(u"filters")
        .toObject()
        .value(u"equipment_filters")
        .toObject()
        .value(u"filters")
        .toObject()
        .keys();
}

// A part of the gear score for "# to <name>", worth 100 trade weight a point.
SearchWeight weightFor(const QString &name, double importance, double cap = 10)
{
    SearchWeight w;
    w.label = name;
    w.key = u"# to "_s + name;
    w.pattern = QRegularExpression(u"^"_s + QRegularExpression::escape(w.key) + u"$"_s);
    w.weight = 1;
    w.importance = importance;
    w.cap = cap;
    return w;
}

// The same for an item property such as "[Base] Armour".
SearchWeight propertyWeight(const QString &key)
{
    SearchWeight w;
    w.label = key;
    w.pattern = QRegularExpression(u"^"_s + QRegularExpression::escape(key) + u"$"_s);
    w.weight = 1;
    w.importance = 1;
    return w;
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

    void thisBaseNeedsAName()
    {
        CustomSearchOptions options;
        options.itemClass = u"Helmets"_s;
        options.matchBase = true;
        const TradeQuery q = customSearch({}, tradeData(), options);
        QVERIFY(!queryOf(q).contains(u"type"));
    }

    void pseudoTotalsCoverEveryKindOfMod()
    {
        StatPick p = pick(u"# to maximum Life"_s, 80);
        p.origin = StatOrigin::Mod;
        const TradeQuery q = customSearch({p}, tradeData(), {});
        QCOMPARE(filterIds(statGroups(q).at(0).toObject()), QStringList{u"pseudo.pseudo_total_life"_s});
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

    void weaponDpsNeedsYourOwnAndAPositiveWeight()
    {
        const Item mace = withStats(u"Two Hand Maces"_s, {{u"[Weapon] Total DPS"_s, 176.4}});
        const WeightedSearch against = weightedSearch(mace, {{u"[Weapon] Total DPS"_s, -1}}, tradeData());
        QVERIFY(equipmentIds(against).isEmpty());

        const Item bare = withStats(u"Two Hand Maces"_s, {{u"# to maximum Life"_s, 5}});
        const WeightedSearch without = weightedSearch(bare, {{u"[Weapon] Total DPS"_s, 1}}, tradeData());
        QVERIFY(equipmentIds(without).isEmpty());
    }

    void weightedSearchNeedsRealWeights()
    {
        const QString armour = u"#% increased Armour"_s;
        const QString speed = u"#% increased Movement Speed"_s;
        const Item helmet = withStats(
            u"Helmets"_s,
            {{u"# to maximum Life"_s, 50}, {u"#% to Fire Resistance"_s, 20}, {armour, 40}, {speed, 10}});
        const Weights weights = {
            {u"# to maximum Life"_s, 1}, {u"#% to Fire Resistance"_s, 0}, {armour, qQNaN()}, {speed, qInf()}};
        const WeightedSearch q = weightedSearch(helmet, weights, tradeData());
        QCOMPARE(filterIds(statGroups(q).at(0).toObject()), QStringList{u"pseudo.pseudo_total_life"_s});
        QCOMPARE(q.current, 50.0);
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

    void similarItems()
    {
        const Item helmet = withStats(u"Helmets"_s, {{u"# to maximum Life"_s, 50},
                                                     {u"#% to Fire Resistance"_s, 20},
                                                     {u"#% increased Movement Speed"_s, 10},
                                                     {u"#% increased Armour"_s, 40},
                                                     {u"[Base] Armour"_s, 300},
                                                     {u"mystery #"_s, 3}});
        SimilarSearchOptions options;
        options.preferLocal = true;
        const SimilarSearch q = similarSearch(helmet, tradeData(), options);
        const QJsonObject group = statGroups(q).at(0).toObject();
        QCOMPARE(group.value(u"type").toString(), u"count"_s);
        QCOMPARE(filterIds(group),
                 QStringList({u"explicit.stat_ms"_s, u"explicit.stat_armour_local"_s,
                              u"pseudo.pseudo_total_life"_s, u"pseudo.pseudo_total_fire_resistance"_s}));
        // a listing may be without one of four
        QCOMPARE(group.value(u"value").toObject(), minimum(3));
        QCOMPARE(q.need, 3);
        QCOMPARE(q.total, 4);
        QCOMPARE(valueOf(group, u"pseudo.pseudo_total_life"_s), minimum(50));
        QCOMPARE(valueOf(group, u"explicit.stat_armour_local"_s), minimum(40));
        QCOMPARE(filterOf(q, u"equipment_filters", u"ar"), minimum(270));
        QCOMPARE(q.unmatched, QStringList{u"mystery #"_s});
        QCOMPARE(q.body.value(u"sort").toObject(), QJsonObject({{u"price"_s, u"asc"_s}}));
        QCOMPARE(q.levelLabel, u"your values or higher"_s);

        const SimilarSearch small =
            similarSearch(withStats(u"Helmets"_s, {{u"# to maximum Life"_s, 50}}), tradeData());
        QCOMPARE(statGroups(small).at(0).toObject().value(u"value").toObject(), minimum(1));
    }

    void similarLeavesRunesOut()
    {
        const Item helmet = parseItem(u"Item Class: Helmets\nRarity: Rare\nA\nB\n--------\nArmour: 300\n"
                                      "--------\n+12% to Fire Resistance (rune)\n--------\n"
                                      "+50 to maximum Life\n+20% to Fire Resistance\n"
                                      "10% increased Armour (rune)\n20% increased Armour"_s)
                                .value();
        SimilarSearchOptions options;
        options.preferLocal = true;
        const SimilarSearch q = similarSearch(helmet, tradeData(), options);
        const QJsonObject group = statGroups(q).at(0).toObject();
        QCOMPARE(valueOf(group, u"explicit.stat_armour_local"_s), minimum(20));
        QCOMPARE(valueOf(group, u"pseudo.pseudo_total_fire_resistance"_s), minimum(20));
        QCOMPARE(q.skipped, QStringList({u"#% to Fire Resistance"_s, u"#% increased Armour"_s}));
    }

    void similarRelaxes()
    {
        const Item gloves =
            parseItem(u"Item Class: Gloves\nRarity: Rare\nDoom Vise\nSorcerer Gloves\n--------\n"
                      "Energy Shield: 280 (augmented)\n--------\n"
                      "18% increased Armour, Evasion and Energy Shield (rune)\n--------\n"
                      "113% increased Energy Shield\n+33 to maximum Life\n"
                      "+39% to Lightning Resistance\n+45% to Cold Resistance"_s)
                .value();
        SimilarSearchOptions options;
        options.preferLocal = true;
        const SimilarSearch exact = similarSearch(gloves, tradeData(), options);
        // 280 * (100 + 113) / (100 + 113 + 18) = 258.18 without the rune, and 90% of that
        QCOMPARE(filterOf(exact, u"equipment_filters", u"es"), minimum(232));
        QCOMPARE(queryOf(exact).value(u"status").toObject().value(u"option").toString(), u"available"_s);
        QCOMPARE(valueOf(statGroups(exact).at(0).toObject(), u"pseudo.pseudo_total_life"_s), minimum(33));

        options.level = 2;
        const SimilarSearch looser = similarSearch(gloves, tradeData(), options);
        QCOMPARE(valueOf(statGroups(looser).at(0).toObject(), u"pseudo.pseudo_total_life"_s), minimum(26));
        QCOMPARE(filterOf(looser, u"equipment_filters", u"es"), minimum(185));
        QVERIFY(looser.need <= exact.need);
        QCOMPARE(looser.level, 2);
        QCOMPARE(looser.levelLabel, u"80% of your values"_s);

        options.level = 99;
        QCOMPARE(similarSearch(gloves, tradeData(), options).level, similarSearchLevels() - 1);

        // small values never round down to nothing
        SimilarSearchOptions loosest;
        loosest.level = 3;
        const SimilarSearch tiny =
            similarSearch(withStats(u"Helmets"_s, {{u"# to maximum Life"_s, 1}}), tradeData(), loosest);
        QCOMPARE(firstValue(tiny), minimum(1));

        // a negative value is a maximum
        const SimilarSearch reduced = similarSearch(
            withStats(u"Boots"_s, {{u"#% increased Movement Speed"_s, -10}}), tradeData(), loosest);
        QCOMPARE(firstValue(reduced), QJsonObject({{u"max"_s, -6.5}}));
    }

    void similarUniques()
    {
        const Item belt =
            parseItem(
                u"Item Class: Belts\nRarity: Unique\nMageblood\nHeavy Belt\n--------\n+50 to maximum Life"_s)
                .value();
        const SimilarSearch q = similarSearch(belt, tradeData());
        QVERIFY(q.unique);
        QCOMPARE(queryOf(q).value(u"name").toString(), u"Mageblood"_s);
        QCOMPARE(queryOf(q).value(u"type").toString(), u"Heavy Belt"_s);
        QVERIFY(!queryOf(q).value(u"filters").toObject().contains(u"type_filters"));
        QCOMPARE(statGroups(q).size(), 1);

        SimilarSearchOptions last;
        last.level = similarSearchLevels() - 1;
        const SimilarSearch nameOnly = similarSearch(belt, tradeData(), last);
        QCOMPARE(nameOnly.need, 0);
        QCOMPARE(statGroups(nameOnly), QJsonArray());
    }

    void looserSearchesMayMissMore()
    {
        const QStringList names = {u"Alpha"_s, u"Bravo"_s,   u"Charlie"_s, u"Delta"_s,
                                   u"Echo"_s,  u"Foxtrot"_s, u"Golf"_s,    u"Hotel"_s};
        const TradeData data = tradeDataFor(names);
        QCOMPARE(similarSearchLevels(), 4);

        // how many of n stats a listing has to have, at each level
        const int needed[4][8] = {
            {1, 2, 3, 3, 4, 5, 6, 7},
            {1, 1, 2, 2, 3, 4, 5, 6},
            {1, 1, 2, 2, 3, 4, 4, 5},
            {1, 1, 1, 2, 2, 3, 3, 4},
        };
        for (int level = 0; level < similarSearchLevels(); ++level) {
            for (int n = 1; n <= 8; ++n) {
                StatMap stats;
                for (int i = 0; i < n; ++i)
                    stats.add(u"# to "_s + names.at(i), 100);
                SimilarSearchOptions options;
                options.level = level;
                const SimilarSearch q = similarSearch(withStats(u"Helmets"_s, stats), data, options);
                QCOMPARE(q.total, n);
                QVERIFY2(q.need == needed[level][n - 1],
                         qPrintable(u"level %1, %2 stats: %3 needed"_s.arg(level).arg(n).arg(q.need)));
            }
        }
    }

    void looserSearchesAskForLess()
    {
        const Item helmet = withStats(u"Helmets"_s, {{u"# to maximum Life"_s, 100}});
        const double shares[] = {100, 90, 80, 65};
        for (int level = 0; level < similarSearchLevels(); ++level) {
            SimilarSearchOptions options;
            options.level = level;
            QCOMPARE(firstValue(similarSearch(helmet, tradeData(), options)), minimum(shares[level]));
        }

        SimilarSearchOptions below;
        below.level = -3;
        QCOMPARE(similarSearch(helmet, tradeData(), below).level, 0);
    }

    void smallValuesKeepTheirDecimals()
    {
        const auto minFor = [](double value, int level) {
            SimilarSearchOptions options;
            options.level = level;
            return firstValue(similarSearch(withStats(u"Helmets"_s, {{u"# to maximum Life"_s, value}}),
                                            tradeData(), options));
        };
        QCOMPARE(minFor(2.5, 0), minimum(2.5));
        QCOMPARE(minFor(3.5, 0), minimum(3));
        // only a whole point or more is kept from rounding down to nothing
        QCOMPARE(minFor(0.4, 3), minimum(0.26));
    }

    void noStatsToSearchFor()
    {
        const SimilarSearch q = similarSearch(withStats(u"Helmets"_s, {{u"mystery #"_s, 3}}), tradeData());
        QCOMPARE(q.total, 0);
        QCOMPARE(statGroups(q), QJsonArray());
    }

    void weaponPropertiesBesidesDps()
    {
        const Item mace = withStats(u"Two Hand Maces"_s, {{u"[Weapon] Total DPS"_s, 200},
                                                          {u"[Weapon] Physical DPS"_s, 150},
                                                          {u"[Weapon] Elemental DPS"_s, 50},
                                                          {u"[Weapon] Attacks per Second"_s, 1.2},
                                                          {u"[Weapon] Critical Hit Chance"_s, 5},
                                                          {u"[Base] Runic Ward"_s, 100},
                                                          {u"[Base] Block Chance"_s, 25},
                                                          {u"[Base] Armour"_s, 300}});
        const SimilarSearch q = similarSearch(mace, tradeData());
        QCOMPARE(equipmentIds(q), QStringList({u"ar"_s, u"dps"_s}));
        QCOMPARE(filterOf(q, u"equipment_filters", u"dps"), minimum(180));
    }

    void uniquesAreSearchedByName()
    {
        // no base type of its own, so none to ask for
        const Item ring =
            parseItem(u"Item Class: Rings\nRarity: Unique\nSeal of Gold\n--------\n+10 to maximum Life"_s)
                .value();
        const SimilarSearch byName = similarSearch(ring, tradeData());
        QCOMPARE(queryOf(byName).value(u"name").toString(), u"Seal of Gold"_s);
        QVERIFY(!queryOf(byName).contains(u"type"));

        Item nameless;
        nameless.itemClass = u"Rings"_s;
        nameless.rarity = u"Unique"_s;
        nameless.slot = u"Ring"_s;
        nameless.stats = {{u"# to maximum Life"_s, 10}};
        const SimilarSearch bare = similarSearch(nameless, tradeData());
        QVERIFY(bare.unique);
        QVERIFY(!queryOf(bare).contains(u"name"));

        // the base's defences depend on the roll, not on what makes it that item
        const Item cap = parseItem(u"Item Class: Helmets\nRarity: Unique\nGoldrim\nLeather Cap\n--------\n"
                                   "Evasion Rating: 100\n--------\n+30 to maximum Life"_s)
                             .value();
        QVERIFY(equipmentIds(similarSearch(cap, tradeData())).isEmpty());
    }

    void similarSkipsIgnoredStats()
    {
        const Item helmet = withStats(
            u"Helmets"_s, {{u"# to maximum Life"_s, 50},
                           {u"#% to Fire Resistance"_s, 30},
                           {u"#% to Cold Resistance"_s, 20},
                           {u"Allies in your Presence have #% to all Elemental Resistances"_s, 10}});
        SimilarSearchOptions options;
        options.ignoreRules = {{u"res"_s, {}, IgnoreRule::Mode::Ignore}};
        const SimilarSearch q = similarSearch(helmet, tradeData(), options);
        QCOMPARE(q.ignored, QStringList({u"#% to Fire Resistance"_s, u"#% to Cold Resistance"_s}));
        QCOMPARE(filterIds(statGroups(q).at(0).toObject()), QStringList{u"pseudo.pseudo_total_life"_s});
        // plain ignore rules don't add a group
        QCOMPARE(statGroups(q).size(), 1);

        options.ignoreRules = {{u"res"_s, {}, IgnoreRule::Mode::Exclude}};
        const SimilarSearch excluding = similarSearch(helmet, tradeData(), options);
        QCOMPARE(statGroups(excluding).at(1).toObject().value(u"type").toString(), u"not"_s);
        QCOMPARE(filterIds(statGroups(excluding).at(1).toObject()),
                 QStringList{u"pseudo.pseudo_total_fire_resistance"_s});
    }

    void craftBases()
    {
        using namespace tradefixtures;
        const TradeData data{
            StatIndex::fromJson(response({
                group(u"pseudo"_s,
                      {
                          stat(u"pseudo.pseudo_number_of_empty_prefix_mods"_s, u"# Empty Prefix Modifiers"_s,
                               u"pseudo"_s),
                          stat(u"pseudo.pseudo_number_of_empty_suffix_mods"_s, u"# Empty Suffix Modifiers"_s,
                               u"pseudo"_s),
                          stat(u"pseudo.pseudo_total_life"_s, u"+# total maximum Life"_s, u"pseudo"_s),
                      }),
                group(u"explicit"_s,
                      {stat(u"explicit.stat_ms"_s, u"#% increased Movement Speed"_s, u"explicit"_s)}),
            })),
            {}};
        const Item boots = withStats(u"Boots"_s, {{u"[Base] Energy Shield"_s, 400}});
        const QList<KeyStat> keyStats = {
            {u"#% increased Movement Speed"_s, u"Movement speed"_s, 9, 35},
            {u"# to maximum Life"_s, u"Life"_s, 8, 100},
            {u"# to Spirit"_s, u"Spirit"_s, 5, 4},
        };
        const CraftBaseSearch q = craftBaseSearch(boots, keyStats, data);
        QCOMPARE(filterOf(q, u"type_filters", u"rarity"), QJsonObject({{u"option"_s, u"rare"_s}}));
        QCOMPARE(filterOf(q, u"type_filters", u"category").value(u"option").toString(), u"armour.boots"_s);
        QCOMPARE(filterOf(q, u"misc_filters", u"corrupted"), QJsonObject({{u"option"_s, u"false"_s}}));
        const QJsonObject wanted = statGroups(q).at(0).toObject();
        QCOMPARE(wanted.value(u"type").toString(), u"and"_s);
        QCOMPARE(filterIds(wanted), QStringList({u"explicit.stat_ms"_s, u"pseudo.pseudo_total_life"_s}));
        QCOMPARE(valueOf(wanted, u"explicit.stat_ms"_s), minimum(19));
        QCOMPARE(valueOf(wanted, u"pseudo.pseudo_total_life"_s), minimum(55));
        const QJsonObject openAffix = statGroups(q).at(1).toObject();
        QCOMPARE(openAffix.value(u"type").toString(), u"count"_s);
        QCOMPARE(openAffix.value(u"filters").toArray().size(), 2);
        QVERIFY(q.emptyAffixFilter);
        QCOMPARE(filterOf(q, u"equipment_filters", u"es"), minimum(360));
        QCOMPARE(q.keyStats, QStringList({u"Movement speed"_s, u"Life"_s, u"Spirit"_s}));
        QCOMPARE(q.unmatched, QStringList{u"# to Spirit"_s});

        // without the pseudo stats there's no asking for an open affix
        const CraftBaseSearch plain = craftBaseSearch(boots, keyStats, tradeData());
        QVERIFY(!plain.emptyAffixFilter);
        QCOMPARE(statGroups(plain).size(), 1);
    }

    void craftBaseRollsAreAboutHalfOfTheBest()
    {
        const TradeData data = tradeDataFor({u"Alpha"_s, u"Bravo"_s, u"Charlie"_s, u"Delta"_s});
        const QList<KeyStat> keyStats = {
            {u"# to Alpha"_s, u"Alpha"_s, 9, 1},
            {u"# to Bravo"_s, u"Bravo"_s, 9, 4},
            {u"# to Charlie"_s, u"Charlie"_s, 9, 10},
            {u"# to Delta"_s, u"Delta"_s, 9, 20},
        };
        const CraftBaseSearch q = craftBaseSearch(withStats(u"Boots"_s, {}), keyStats, data);
        const QJsonObject wanted = statGroups(q).at(0).toObject();
        // never nothing, and a little over half from 5 up
        QCOMPARE(valueOf(wanted, u"explicit.alpha"_s), minimum(1));
        QCOMPARE(valueOf(wanted, u"explicit.bravo"_s), minimum(2));
        QCOMPARE(valueOf(wanted, u"explicit.charlie"_s), minimum(5));
        QCOMPARE(valueOf(wanted, u"explicit.delta"_s), minimum(11));
    }

    void craftBaseWithoutKeyStats()
    {
        const CraftBaseSearch q = craftBaseSearch(withStats(u"Boots"_s, {}), {}, tradeData());
        QCOMPARE(statGroups(q), QJsonArray());
        QVERIFY(q.keyStats.isEmpty());
    }

    void craftBaseKeyStatsAreMods()
    {
        const KeyStat armour{u"[Base] Armour"_s, u"Armour"_s, 9, 100};
        const CraftBaseSearch q = craftBaseSearch(withStats(u"Boots"_s, {}), {armour}, tradeData());
        QCOMPARE(q.unmatched, QStringList{u"[Base] Armour"_s});
        QCOMPARE(statGroups(q), QJsonArray());
    }

    void craftBaseLeavesWhatComesWithTheBase()
    {
        const Item mace = withStats(u"Two Hand Maces"_s, {{u"[Weapon] Total DPS"_s, 200},
                                                          {u"[Weapon] Physical DPS"_s, 150},
                                                          {u"[Base] Runic Ward"_s, 100},
                                                          {u"[Base] Block Chance"_s, 25},
                                                          {u"[Base] Armour"_s, 300}});
        QCOMPARE(equipmentIds(craftBaseSearch(mace, {}, tradeData())), QStringList{u"ar"_s});
    }

    void gearSearchForTheBuild()
    {
        using namespace tradefixtures;
        const TradeData data{
            StatIndex::fromJson(response({
                group(
                    u"explicit"_s,
                    {
                        stat(u"explicit.minion_dmg"_s, u"Minions deal #% increased Damage"_s, u"explicit"_s),
                        stat(u"explicit.minion_crit"_s, u"Minions have #% increased Critical Hit Chance"_s,
                             u"explicit"_s),
                        stat(u"explicit.ally_crit"_s,
                             u"Allies in your Presence have #% increased Critical Hit Chance"_s,
                             u"explicit"_s),
                        stat(u"explicit.mana"_s, u"+# to maximum Mana"_s, u"explicit"_s),
                        stat(u"explicit.int"_s, u"+# to Intelligence"_s, u"explicit"_s),
                        stat(u"explicit.phys"_s, u"Adds # to # Physical Damage to Attacks"_s, u"explicit"_s),
                    }),
                group(u"implicit"_s, {stat(u"implicit.mana"_s, u"+# to maximum Mana"_s, u"implicit"_s)}),
                group(u"pseudo"_s,
                      {stat(u"pseudo.pseudo_total_mana"_s, u"+# total maximum Mana"_s, u"pseudo"_s)}),
            })),
            {}};
        const Item ring = parseItem(u"Item Class: Rings\nRarity: Rare\nPlague Circle\nIron Ring\n--------\n"
                                    "Item Level: 80\n--------\n+175 to maximum Mana\n"
                                    "Minions deal 27% increased Damage\n"
                                    "Minions have 44% increased Critical Hit Chance\n+20 to Intelligence"_s)
                              .value();
        const QList<SearchWeight> weights = searchWeights(u"Ring"_s, {.build = u"minion-es"_s});
        const GearSearch q = gearSearch(ring, weights, data);
        const QJsonObject group = statGroups(q).at(0).toObject();
        QCOMPARE(group.value(u"type").toString(), u"weight"_s);
        // heaviest first; implicit and explicit mana both count, pseudo mana would count it twice
        QCOMPARE(filterIds(group),
                 QStringList({u"explicit.minion_dmg"_s, u"explicit.minion_crit"_s, u"explicit.ally_crit"_s,
                              u"explicit.int"_s, u"explicit.mana"_s, u"implicit.mana"_s}));
        QCOMPARE(valueOf(group, u"explicit.minion_dmg"_s), weight(28));
        // 27 * 28 + 44 * 12.5 + 175 * 2 + 20 * 8.57
        QCOMPARE(q.current, 1827.4);
        QCOMPARE(q.groupMin, 1828.0);
        QCOMPARE(group.value(u"value").toObject(), minimum(1828));
        QCOMPARE(q.parts, QStringList({u"Minion damage"_s, u"Minion crit"_s, u"Mana"_s, u"Intelligence"_s}));
        QCOMPARE(q.filters, 6);
        QCOMPARE(q.dropped, 0);
        QCOMPARE(q.body.value(u"sort").toObject(), QJsonObject({{u"statgroup.0"_s, u"desc"_s}}));

        GearSearchOptions close;
        close.share = 0.9;
        QVERIFY(gearSearch(ring, weights, data, close).groupMin < q.current);

        const Item gloves = parseItem(u"Item Class: Gloves\nRarity: Rare\nDoom Vise\nSirenscale Gloves\n"
                                      "--------\nEnergy Shield: 280\n--------\nItem Level: 80\n--------\n"
                                      "+33 to maximum Life"_s)
                                .value();
        GearSearchOptions options;
        options.baseShare = 0.85;
        const GearSearch g =
            gearSearch(gloves, searchWeights(u"Gloves"_s, {.build = u"minion-es"_s}), data, options);
        QCOMPARE(filterOf(g, u"equipment_filters", u"es"), minimum(238));
        QCOMPARE(g.parts.first(), u"Energy Shield at least 238"_s);
    }

    void gearSearchKeepsEveryBaseDefence()
    {
        // "Armour / Evasion" covers both, so a hybrid base keeps both
        const Item coat =
            withStats(u"Body Armours"_s, {{u"[Base] Armour"_s, 500}, {u"[Base] Evasion Rating"_s, 400}});
        const GearSearch q = gearSearch(coat, searchWeights(u"Body Armour"_s), tradeData());
        QCOMPARE(filterOf(q, u"equipment_filters", u"ar"), minimum(400));
        QCOMPARE(filterOf(q, u"equipment_filters", u"ev"), minimum(320));
        QVERIFY(q.parts.contains(u"Evasion Rating at least 320"_s));
    }

    void gearSearchNeedsKeyStats()
    {
        using namespace tradefixtures;
        const TradeData data{
            StatIndex::fromJson(response({group(
                u"explicit"_s,
                {
                    stat(u"explicit.es_pct"_s, u"#% increased maximum Energy Shield"_s, u"explicit"_s),
                    stat(u"explicit.minion_dmg"_s, u"Minions deal #% increased Damage"_s, u"explicit"_s),
                    stat(u"explicit.minion_dmg_hit"_s,
                         u"Minions deal #% increased Damage if you've Hit Recently"_s, u"explicit"_s),
                    stat(u"explicit.minion_speed"_s, u"Minions have #% increased Attack Speed"_s,
                         u"explicit"_s),
                    stat(u"explicit.int"_s, u"+# to Intelligence"_s, u"explicit"_s),
                })})),
            {}};
        const GearSearch q = gearSearch(jewel(), searchWeights(u"Jewel"_s, {.build = u"minion-es"_s}), data);
        const QJsonObject count = statGroups(q).at(1).toObject();
        QCOMPARE(count.value(u"type").toString(), u"count"_s);
        // as many as the jewel has, up to two
        QCOMPARE(count.value(u"value").toObject(), minimum(2));
        QCOMPARE(q.needKeyStats, 2);
        // the conditional version doesn't count as the key stat
        QCOMPARE(filterIds(count), QStringList({u"explicit.minion_dmg"_s, u"explicit.es_pct"_s}));
        QCOMPARE(valueOf(count, u"explicit.minion_dmg"_s), minimum(6));
        QCOMPARE(q.keyStats, QStringList({u"Minion damage"_s, u"% Energy Shield"_s}));
        // but it weighs less
        const QJsonObject weighted = statGroups(q).at(0).toObject();
        QVERIFY(valueOf(weighted, u"explicit.minion_dmg_hit"_s).value(u"weight").toDouble() <
                valueOf(weighted, u"explicit.minion_dmg"_s).value(u"weight").toDouble());
    }

    void gearSearchShrinks()
    {
        using namespace tradefixtures;
        const TradeData data{
            StatIndex::fromJson(response({group(
                u"explicit"_s,
                {
                    stat(u"explicit.es_pct"_s, u"#% increased maximum Energy Shield"_s, u"explicit"_s),
                    stat(u"implicit.es_pct"_s, u"#% increased maximum Energy Shield"_s, u"implicit"_s),
                    stat(u"explicit.minion_dmg"_s, u"Minions deal #% increased Damage"_s, u"explicit"_s),
                    stat(u"explicit.minion_dmg_hit"_s,
                         u"Minions deal #% increased Damage if you've Hit Recently"_s, u"explicit"_s),
                    stat(u"explicit.minion_speed"_s, u"Minions have #% increased Attack Speed"_s,
                         u"explicit"_s),
                    stat(u"explicit.int"_s, u"+# to Intelligence"_s, u"explicit"_s),
                })})),
            {}};
        const QList<SearchWeight> weights = searchWeights(u"Jewel"_s, {.build = u"minion-es"_s});
        const GearSearch big = gearSearch(jewel(), weights, data);
        QCOMPARE(filterCount(big), 9);

        GearSearchOptions smaller;
        smaller.explicitOnly = true;
        smaller.maxFilters = 3;
        smaller.keyStatShare = 0;
        const GearSearch mid = gearSearch(jewel(), weights, data, smaller);
        QCOMPARE(filterCount(mid), 3);
        QCOMPARE(mid.dropped, 2);
        QVERIFY(!filterIds(statGroups(mid).at(0).toObject()).contains(u"implicit.es_pct"_s));

        GearSearchOptions smallest;
        smallest.explicitOnly = true;
        smallest.countOnly = true;
        const GearSearch small = gearSearch(jewel(), weights, data, smallest);
        QVERIFY(small.countOnly);
        QCOMPARE(statGroups(small).size(), 1);
        const QJsonObject count = statGroups(small).at(0).toObject();
        QCOMPARE(count.value(u"type").toString(), u"count"_s);
        QCOMPARE(filterIds(count), QStringList({u"explicit.minion_dmg"_s, u"explicit.es_pct"_s}));
        QCOMPARE(count.value(u"value").toObject(), minimum(2));
        QCOMPARE(small.needKeyStats, 2);
        QCOMPARE(small.body.value(u"sort").toObject(), QJsonObject({{u"price"_s, u"asc"_s}}));
        // and two of them from one without any, so it isn't every jewel
        const Item plain = parseItem(u"Item Class: Jewels\nRarity: Rare\nHonour Stone\nSapphire\n--------\n"
                                     "+8 to Intelligence"_s)
                               .value();
        QCOMPARE(gearSearch(plain, weights, data, smallest).needKeyStats, 2);
    }

    void gearSearchOptionsOfZeroMeanTheDefaults()
    {
        const Item boots = withStats(u"Boots"_s, {{u"[Base] Armour"_s, 100}});
        GearSearchOptions noBaseShare;
        noBaseShare.baseShare = 0;
        const GearSearch q =
            gearSearch(boots, {propertyWeight(u"[Base] Armour"_s)}, tradeData(), noBaseShare);
        QCOMPARE(filterOf(q, u"equipment_filters", u"ar"), minimum(80));

        QStringList names;
        QList<SearchWeight> weights;
        for (int i = 0; i < 42; ++i) {
            names << u"S%1"_s.arg(i);
            weights << weightFor(names.last(), 1);
        }
        GearSearchOptions noLimit;
        noLimit.maxFilters = 0;
        const GearSearch many = gearSearch(withStats(u"Rings"_s, {}), weights, tradeDataFor(names), noLimit);
        QCOMPARE(many.filters, 40);
        QCOMPARE(many.dropped, 2);
    }

    void gearSearchSkipsWhatTheItemLacks()
    {
        const Item helmet = withStats(u"Helmets"_s, {{u"# to Alpha"_s, 5}});
        const QList<SearchWeight> weights = {propertyWeight(u"[Base] Armour"_s), weightFor(u"Alpha"_s, 1),
                                             weightFor(u"Bravo"_s, 1)};
        const GearSearch q = gearSearch(helmet, weights, tradeDataFor({u"Alpha"_s, u"Bravo"_s}));
        // no base armour to ask for, and a property isn't a mod to weigh
        QVERIFY(equipmentIds(q).isEmpty());
        QCOMPARE(q.filters, 2);
        QCOMPARE(q.parts, QStringList{u"Alpha"_s});
    }

    void gearSearchKeyStatsAreTheMostImportant()
    {
        const TradeData data =
            tradeDataFor({u"Alpha"_s, u"Bravo"_s, u"Charlie"_s, u"Delta while Moving"_s, u"Echo"_s});
        // least important first, so the order comes from the sort
        const QList<SearchWeight> weights = {
            weightFor(u"Bravo"_s, 4), weightFor(u"Charlie"_s, 3),  weightFor(u"Delta while Moving"_s, 8),
            weightFor(u"Echo"_s, 6),  weightFor(u"Alpha"_s, 9, 1),
        };
        const Item ring = withStats(u"Rings"_s, {{u"# to Alpha"_s, 1},
                                                 {u"# to Bravo"_s, 5},
                                                 {u"# to Charlie"_s, 5},
                                                 {u"# to Delta while Moving"_s, 5},
                                                 {u"# to Echo"_s, 5}});
        GearSearchOptions options;
        options.keyStatShare = 0.5;
        const GearSearch q = gearSearch(ring, weights, data, options);
        // not Charlie, who isn't important enough, nor Delta, who has no unconditional version
        QCOMPARE(q.keyStats, QStringList({u"Alpha"_s, u"Echo"_s, u"Bravo"_s}));
        const QJsonObject count = statGroups(q).at(1).toObject();
        QCOMPARE(filterIds(count),
                 QStringList({u"explicit.alpha"_s, u"explicit.echo"_s, u"explicit.bravo"_s}));
        // half of a good roll, and never nothing
        QCOMPARE(valueOf(count, u"explicit.alpha"_s), minimum(1));
        QCOMPARE(valueOf(count, u"explicit.bravo"_s), minimum(5));
        // up to two, however many it has
        QCOMPARE(q.needKeyStats, 2);
        QCOMPARE(count.value(u"value").toObject(), minimum(2));
    }

    void gearSearchAsksForOnlyTheKeyStatsYouHave()
    {
        const TradeData data = tradeDataFor({u"Alpha"_s, u"Bravo"_s});
        const QList<SearchWeight> weights = {weightFor(u"Alpha"_s, 5), weightFor(u"Bravo"_s, 5)};

        const GearSearch none = gearSearch(withStats(u"Rings"_s, {}), weights, data);
        QCOMPARE(none.keyStats, QStringList({u"Alpha"_s, u"Bravo"_s}));
        QCOMPARE(none.needKeyStats, 0);
        QCOMPARE(statGroups(none).size(), 1);

        // a negative roll is not having it
        const Item cursed = withStats(u"Rings"_s, {{u"# to Alpha"_s, -5}, {u"# to Bravo"_s, -5}});
        const GearSearch negative = gearSearch(cursed, weights, data);
        QCOMPARE(negative.needKeyStats, 0);
        QCOMPARE(statGroups(negative).size(), 1);

        // and runes can be swapped, so they don't count either
        const Item runed = parseItem(u"Item Class: Rings\nRarity: Rare\nA\nB\n--------\n"
                                     "+10 to Alpha (rune)\n--------\n+5 to Bravo"_s)
                               .value();
        const GearSearch q = gearSearch(runed, weights, data);
        QCOMPARE(q.needKeyStats, 1);
        QCOMPARE(q.current, 500.0);
        QCOMPARE(q.parts, QStringList{u"Bravo"_s});
    }

    void gearSearchCountsConditionalStatsForLess()
    {
        const GearSearch q =
            gearSearch(withStats(u"Rings"_s, {{u"# to Delta while Moving"_s, 10}}),
                       {weightFor(u"Delta while Moving"_s, 1)}, tradeDataFor({u"Delta while Moving"_s}));
        QCOMPARE(q.current, 600.0);
    }

    void gearSearchCountsResistsOnlyUpToWhatYouNeed()
    {
        SearchWeight fire = weightFor(u"Fire"_s, 1);
        fire.resist = Element::Fire;
        const Item ring = withStats(u"Rings"_s, {{u"# to Fire"_s, 50}});
        const TradeData data = tradeDataFor({u"Fire"_s});

        fire.need = 20;
        QCOMPARE(gearSearch(ring, {fire}, data).current, 2000.0);
        fire.need = std::nullopt;
        QCOMPARE(gearSearch(ring, {fire}, data).current, 5000.0);

        // capped already, so it adds nothing
        fire.need = -5;
        const GearSearch capped = gearSearch(ring, {fire}, data);
        QCOMPARE(capped.current, 0.0);
        QVERIFY(capped.parts.isEmpty());
    }

    void gearSearchWantsAtLeastOnePoint()
    {
        GearSearchOptions within;
        within.share = 0.5;
        const GearSearch q = gearSearch(withStats(u"Rings"_s, {}), {weightFor(u"Alpha"_s, 1)},
                                        tradeDataFor({u"Alpha"_s}), within);
        QCOMPARE(q.current, 0.0);
        QCOMPARE(q.groupMin, 1.0);
    }

    void countOnlyNeedsKeyStats()
    {
        GearSearchOptions countOnly;
        countOnly.countOnly = true;
        const Item ring = withStats(u"Rings"_s, {{u"# to Alpha"_s, 5}});
        const GearSearch minor =
            gearSearch(ring, {weightFor(u"Alpha"_s, 3)}, tradeDataFor({u"Alpha"_s}), countOnly);
        QVERIFY(!minor.countOnly);
        QCOMPARE(minor.body.value(u"sort").toObject(), QJsonObject({{u"statgroup.0"_s, u"desc"_s}}));

        // the explicit version of a key stat is asked for, at a roll of at least 1
        using namespace tradefixtures;
        const TradeData both{
            StatIndex::fromJson(response({
                group(u"implicit"_s, {stat(u"implicit.alpha"_s, u"+# to Alpha"_s, u"implicit"_s)}),
                group(u"explicit"_s, {stat(u"explicit.alpha"_s, u"+# to Alpha"_s, u"explicit"_s)}),
            })),
            {}};
        const GearSearch q = gearSearch(ring, {weightFor(u"Alpha"_s, 5, 1)}, both, countOnly);
        QVERIFY(q.countOnly);
        const QJsonObject count = statGroups(q).at(0).toObject();
        QCOMPARE(filterIds(count), QStringList{u"explicit.alpha"_s});
        QCOMPARE(valueOf(count, u"explicit.alpha"_s), minimum(1));
    }

    void countOnlyWantsUpToThreeKeyStats()
    {
        const QStringList names = {u"Alpha"_s, u"Bravo"_s, u"Charlie"_s, u"Delta"_s};
        QList<SearchWeight> weights;
        for (const QString &name : names)
            weights << weightFor(name, 5);
        GearSearchOptions countOnly;
        countOnly.countOnly = true;

        const auto needed = [&](const StatMap &stats) {
            return gearSearch(withStats(u"Rings"_s, stats), weights, tradeDataFor(names), countOnly)
                .needKeyStats;
        };
        QCOMPARE(
            needed(
                {{u"# to Alpha"_s, 1}, {u"# to Bravo"_s, 1}, {u"# to Charlie"_s, 1}, {u"# to Delta"_s, 1}}),
            3);
        QCOMPARE(needed({{u"# to Alpha"_s, 1}}), 1);
        // two from one without any, so it isn't every item
        QCOMPARE(needed({}), 2);
    }

    void pobSearchForTheBuild()
    {
        PobSearchOptions options;
        options.itemClass = u"Helmets"_s;
        options.min = 140.08;
        const PobSearch q = pobSearch({{u"explicit.life"_s, 2},
                                       {u"explicit.speed"_s, 5},
                                       {u"explicit.gone"_s, 9},
                                       {u"explicit.phys"_s, 3}},
                                      pobSite(), options);
        QCOMPARE(statGroups(q).size(), 1);
        const QJsonObject group = statGroups(q).at(0).toObject();
        QCOMPARE(group.value(u"type").toString(), u"weight"_s);
        // the heaviest first
        QCOMPARE(filterIds(group),
                 QStringList({u"explicit.speed"_s, u"explicit.phys"_s, u"explicit.life"_s}));
        QCOMPARE(valueOf(group, u"explicit.phys"_s), weight(3));
        QCOMPARE(group.value(u"value").toObject(), minimum(140.1));
        QCOMPARE(q.groupMin, 140.1);
        // one the site doesn't have
        QCOMPARE(q.unmatched, QStringList{u"explicit.gone"_s});
        QCOMPARE(q.filters, 3);
        QCOMPARE(q.dropped, 0);
        QVERIFY(!q.resistsBoosted);
        QCOMPARE(q.needDamage, 0);
        QCOMPARE(filterOf(q, u"type_filters", u"category"), QJsonObject({{u"option"_s, u"armour.helmet"_s}}));
        // not uniques, as Path of Building's own searches
        QCOMPARE(filterOf(q, u"type_filters", u"rarity"), QJsonObject({{u"option"_s, u"nonunique"_s}}));
        QCOMPARE(queryOf(q).value(u"status").toObject(), QJsonObject({{u"option"_s, u"available"_s}}));
        QCOMPARE(q.body.value(u"sort").toObject(), QJsonObject({{u"statgroup.0"_s, u"desc"_s}}));
    }

    void pobSearchCategoryAndMinimum()
    {
        const QList<ModWeight> mods = {{u"explicit.life"_s, 2}};
        PobSearchOptions options;
        // Path of Building's, for a class the site has no category for
        options.itemClass = u"Trinkets"_s;
        options.category = u"armour.focus"_s;
        options.status = ListingStatus::Online;
        const PobSearch q = pobSearch(mods, pobSite(), options);
        QCOMPARE(filterOf(q, u"type_filters", u"category"), QJsonObject({{u"option"_s, u"armour.focus"_s}}));
        QCOMPARE(queryOf(q).value(u"status").toObject(), QJsonObject({{u"option"_s, u"online"_s}}));
        // but the site's own comes first
        options.itemClass = u"Helmets"_s;
        QCOMPARE(filterOf(pobSearch(mods, pobSite(), options), u"type_filters", u"category"),
                 QJsonObject({{u"option"_s, u"armour.helmet"_s}}));

        // with neither, and no minimum to reach
        const PobSearch plain = pobSearch(mods, pobSite());
        const QJsonObject typeFilters =
            queryOf(plain).value(u"filters").toObject().value(u"type_filters").toObject();
        QCOMPARE(typeFilters.value(u"filters").toObject().keys(), QStringList{u"rarity"_s});
        QCOMPARE(plain.groupMin, 0.0);
        QVERIFY(!statGroups(plain).at(0).toObject().value(u"value").toObject().contains(u"min"));
        options.min = -5;
        QCOMPARE(pobSearch(mods, pobSite(), options).groupMin, 0.0);
    }

    void pobSearchSkipsWhatCantCount()
    {
        PobSearchOptions options;
        options.ignoreRules = {{u"res"_s, {}, IgnoreRule::Mode::Ignore}};
        const QList<ModWeight> mods = {{u"explicit.life"_s, 2},
                                       {u"explicit.fire"_s, 4},
                                       {u"explicit.speed"_s, 0},
                                       {u"explicit.crit"_s, -1},
                                       {u"explicit.phys"_s, qInf()}};
        const PobSearch q = pobSearch(mods, pobSite(), options);
        QCOMPARE(filterIds(statGroups(q).at(0).toObject()), QStringList{u"explicit.life"_s});
        QCOMPARE(q.filters, 1);
        QCOMPARE(q.excluded, 0);
        QCOMPARE(statGroups(q).size(), 1);

        // and listings with those excluded are left out
        options.ignoreRules = {{u"res"_s, {}, IgnoreRule::Mode::Exclude}};
        const PobSearch excluding = pobSearch(mods, pobSite(), options);
        QCOMPARE(statGroups(excluding).size(), 2);
        QCOMPARE(statGroups(excluding).at(1).toObject().value(u"type").toString(), u"not"_s);
        QVERIFY(excluding.excluded > 0);

        // nothing to search for
        const PobSearch none = pobSearch({{u"explicit.gone"_s, 3}}, pobSite());
        QCOMPARE(none.filters, 0);
        QVERIFY(statGroups(none).isEmpty());
    }

    void pobSearchKeepsTheHeaviest()
    {
        const QList<ModWeight> mods = {{u"explicit.life"_s, 2},
                                       {u"explicit.speed"_s, 5},
                                       {u"explicit.phys"_s, 3},
                                       {u"explicit.crit"_s, 1}};
        PobSearchOptions options;
        options.maxFilters = 2;
        const PobSearch q = pobSearch(mods, pobSite(), options);
        QCOMPARE(filterIds(statGroups(q).at(0).toObject()),
                 QStringList({u"explicit.speed"_s, u"explicit.phys"_s}));
        QCOMPARE(q.filters, 2);
        QCOMPARE(q.dropped, 2);
        // always one at least
        options.maxFilters = 0;
        QCOMPARE(pobSearch(mods, pobSite(), options).filters, 1);
    }

    void pobSearchHelpsWithResistances()
    {
        PobSearchOptions options;
        options.shortResists = {Element::Fire};
        const QList<ModWeight> mods = {{u"explicit.speed"_s, 5},
                                       {u"explicit.life"_s, 2},
                                       {u"explicit.fire"_s, 0.5},
                                       {u"explicit.cold"_s, 0.5}};
        const PobSearch q = pobSearch(mods, pobSite(), options);
        QVERIFY(q.resistsBoosted);
        const QJsonObject group = statGroups(q).at(0).toObject();
        // every mod that adds to fire, explicit or implicit, is worth half the
        // best mod of a percentage
        QCOMPARE(filterIds(group), QStringList({u"explicit.speed"_s, u"explicit.fire"_s,
                                                u"explicit.fire_cold"_s, u"explicit.all_res"_s,
                                                u"implicit.fire"_s, u"explicit.life"_s, u"explicit.cold"_s}));
        QCOMPARE(valueOf(group, u"explicit.fire"_s), weight(2.5));
        QCOMPARE(valueOf(group, u"implicit.fire"_s), weight(2.5));
        QCOMPARE(valueOf(group, u"explicit.cold"_s), weight(0.5));

        // one worth more already stays so, and doesn't count as the best
        const PobSearch more =
            pobSearch({{u"explicit.speed"_s, 5}, {u"explicit.fire"_s, 8}}, pobSite(), options);
        QCOMPARE(valueOf(statGroups(more).at(0).toObject(), u"explicit.fire"_s), weight(8));
        QCOMPARE(valueOf(statGroups(more).at(0).toObject(), u"explicit.fire_cold"_s), weight(2.5));
        // with no mod of a percentage, it's half the best of them all
        const PobSearch flat = pobSearch({{u"explicit.life"_s, 4}}, pobSite(), options);
        QCOMPARE(valueOf(statGroups(flat).at(0).toObject(), u"explicit.fire"_s), weight(2));

        // all elemental resistances don't add to chaos
        options.shortResists = {Element::Chaos};
        const PobSearch chaos = pobSearch(mods, pobSite(), options);
        QCOMPARE(filterIds(statGroups(chaos).at(0).toObject()),
                 QStringList({u"explicit.speed"_s, u"explicit.chaos"_s, u"explicit.life"_s,
                              u"explicit.fire"_s, u"explicit.cold"_s}));
    }

    void pobSearchLeavesIgnoredResistancesBe()
    {
        const QList<ModWeight> mods = {{u"explicit.speed"_s, 5}, {u"explicit.cold"_s, 0.5}};
        PobSearchOptions options;
        options.shortResists = {Element::Fire, Element::Cold};
        options.ignoreRules = {{{}, u"#% to Fire Resistance"_s, IgnoreRule::Mode::Ignore}};
        const PobSearch q = pobSearch(mods, pobSite(), options);
        QVERIFY(q.resistsBoosted);
        const QStringList ids = filterIds(statGroups(q).at(0).toObject());
        QVERIFY(ids.contains(u"explicit.cold"_s));
        QVERIFY(ids.contains(u"explicit.fire_cold"_s));
        QVERIFY(!ids.contains(u"explicit.fire"_s));
        QVERIFY(!ids.contains(u"implicit.fire"_s));

        options.shortResists = {Element::Fire};
        const PobSearch fire = pobSearch(mods, pobSite(), options);
        QVERIFY(!fire.resistsBoosted);
        QCOMPARE(filterIds(statGroups(fire).at(0).toObject()),
                 QStringList({u"explicit.speed"_s, u"explicit.cold"_s}));

        // and a mod you ignore that would add to it
        options.ignoreRules = {{{}, u"#% to all Elemental Resistances"_s, IgnoreRule::Mode::Ignore}};
        QVERIFY(!filterIds(statGroups(pobSearch(mods, pobSite(), options)).at(0).toObject())
                     .contains(u"explicit.all_res"_s));
    }

    void pobSearchKeepsYourDamage()
    {
        const QList<ModWeight> mods = {{u"explicit.speed"_s, 5},
                                       {u"explicit.phys"_s, 3},
                                       {u"explicit.life"_s, 2.5},
                                       {u"explicit.recoup"_s, 2},
                                       {u"explicit.crit"_s, 1}};
        PobSearchOptions options;
        options.worn = {u"#% increased Attack Speed"_s, u"Adds # to # Physical Damage to Attacks"_s,
                        u"# to maximum Life"_s};
        const PobSearch q = pobSearch(mods, pobSite(), options);
        QCOMPARE(q.needDamage, 2);
        QCOMPARE(statGroups(q).size(), 2);
        const QJsonObject count = statGroups(q).at(1).toObject();
        QCOMPARE(count.value(u"type").toString(), u"count"_s);
        QCOMPARE(count.value(u"value").toObject(), minimum(2));
        // the build's best damage mods, not those that keep you alive
        QCOMPARE(filterIds(count),
                 QStringList({u"explicit.speed"_s, u"explicit.phys"_s, u"explicit.crit"_s}));
        QCOMPARE(valueOf(count, u"explicit.speed"_s), QJsonObject());

        // as many as yours has, up to two
        options.worn << u"#% increased Critical Hit Chance for Attacks"_s;
        QCOMPARE(pobSearch(mods, pobSite(), options).needDamage, 2);
        options.worn = {u"#% increased Attack Speed"_s};
        QCOMPARE(pobSearch(mods, pobSite(), options).needDamage, 1);
        options.worn = {u"# to maximum Life"_s};
        const PobSearch without = pobSearch(mods, pobSite(), options);
        QCOMPARE(without.needDamage, 0);
        QCOMPARE(statGroups(without).size(), 1);
        options.worn = {};
        QCOMPARE(pobSearch(mods, pobSite(), options).needDamage, 0);
    }

    void pobSearchDamageCountsTowardTheLimit()
    {
        const QList<ModWeight> mods = {{u"explicit.speed"_s, 5},
                                       {u"explicit.phys"_s, 3},
                                       {u"explicit.life"_s, 2.5},
                                       {u"explicit.recoup"_s, 2},
                                       {u"explicit.crit"_s, 1}};
        PobSearchOptions options;
        options.maxFilters = 4;
        options.worn = {u"#% increased Attack Speed"_s, u"Adds # to # Physical Damage to Attacks"_s};
        const PobSearch q = pobSearch(mods, pobSite(), options);
        QCOMPARE(filterIds(statGroups(q).at(1).toObject()),
                 QStringList({u"explicit.speed"_s, u"explicit.phys"_s}));
        QCOMPARE(q.filters, 2);
        QCOMPARE(q.dropped, 3);
        QCOMPARE(filterCount(q), 4);
        // without the group, the mods have all the room
        options.worn = {u"# to maximum Life"_s};
        QCOMPARE(pobSearch(mods, pobSite(), options).filters, 4);

        // and the group has eight at most
        using namespace tradefixtures;
        QJsonArray entries;
        QList<ModWeight> many;
        for (int n = 1; n <= 12; ++n) {
            entries << stat(u"explicit.dmg_%1"_s.arg(n), u"#% increased Damage %1"_s.arg(n), u"explicit"_s);
            many << ModWeight{u"explicit.dmg_%1"_s.arg(n), 13.0 - n};
        }
        const TradeData site{StatIndex::fromJson(response({group(u"explicit"_s, entries)})), {}};
        options.maxFilters = 40;
        options.worn = {u"#% increased Damage 1"_s};
        QCOMPARE(filterIds(statGroups(pobSearch(many, site, options)).at(1).toObject()).size(), 8);
        // and a quarter of them, when it has fewer
        options.maxFilters = 12;
        const PobSearch fewer = pobSearch(many, site, options);
        QCOMPARE(filterIds(statGroups(fewer).at(1).toObject()).size(), 3);
        QCOMPARE(fewer.filters, 9);
    }
};

QTEST_GUILESS_MAIN(TestTradeQuery)
#include "tst_tradequery.moc"
