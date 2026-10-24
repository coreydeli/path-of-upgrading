// SPDX-License-Identifier: MIT
#include "core/tradequery.h"

#include "core/numbers.h"
#include "core/statnorm.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <array>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QJsonObject option(const QString &value)
{
    return {{u"option"_s, value}};
}

QJsonObject minMax(std::optional<double> min, std::optional<double> max)
{
    QJsonObject value;
    if (min)
        value.insert(u"min"_s, *min);
    if (max)
        value.insert(u"max"_s, *max);
    return value;
}

QJsonObject statFilter(const QString &id, const QJsonObject &value)
{
    return {{u"id"_s, id}, {u"value"_s, value}, {u"disabled"_s, false}};
}

QJsonObject statGroup(const QString &type, const QJsonArray &filters, const QJsonObject &value = {})
{
    QJsonObject group{{u"type"_s, type}, {u"filters"_s, filters}};
    if (!value.isEmpty())
        group.insert(u"value"_s, value);
    return group;
}

QJsonObject filterGroup(const QJsonObject &filters)
{
    return {{u"filters"_s, filters}};
}

QJsonObject categoryFilter(const QString &category)
{
    return filterGroup({{u"category"_s, option(category)}});
}

void addCategory(QJsonObject &filters, const TradeData &data, const QString &itemClass)
{
    if (const QString category = data.categories.categoryFor(itemClass); !category.isEmpty())
        filters.insert(u"type_filters"_s, categoryFilter(category));
}

QJsonObject newQuery(ListingStatus status)
{
    return {{u"status"_s, option(listingStatusId(status))}};
}

QJsonObject searchBody(const QJsonObject &query, const QString &sortBy, const QString &order)
{
    return {{u"query"_s, query}, {u"sort"_s, QJsonObject{{sortBy, order}}}};
}

// Stats ruled out with "exclude": listings with any of them are left out.
int addExcludeGroup(QJsonObject &query, const QList<IgnoreRule> &rules, const StatIndex &index)
{
    QStringList ids;
    for (const QString &key : excludedKeys(rules)) {
        const auto m = index.match(key);
        if (m && m->kind == StatMatch::Kind::Stat && !ids.contains(m->id))
            ids << m->id;
    }
    if (ids.isEmpty())
        return 0;
    QJsonArray filters;
    for (const QString &id : std::as_const(ids))
        filters.append(QJsonObject{{u"id"_s, id}, {u"disabled"_s, false}});
    QJsonArray stats = query.value(u"stats").toArray();
    stats.append(statGroup(u"not"_s, filters));
    query.insert(u"stats"_s, stats);
    return int(ids.size());
}

QString modTypeFor(StatOrigin origin)
{
    switch (origin) {
    case StatOrigin::Implicit:
        return u"implicit"_s;
    case StatOrigin::Socket:
        return u"augment"_s;
    case StatOrigin::Mod:
    case StatOrigin::Base:
        break;
    }
    return u"explicit"_s;
}

struct Looseness
{
    double share;          // of your values a listing needs
    int (*mayMiss)(int n); // how many of your n stats a listing may be without
    const char16_t *label;
};

const std::array<Looseness, 4> looseness = {{
    {1.0, [](int n) { return n >= 4 ? 1 : 0; }, u"your values or higher"},
    {0.9, [](int n) { return n >= 4   ? 2
                             : n >= 2 ? 1
                                      : 0; }, u"90% of your values"},
    {0.8, [](int n) { return std::max(n >= 2 ? 1 : 0, (n + 2) / 3); }, u"80% of your values"},
    {0.65, [](int n) { return std::max(n >= 2 ? 1 : 0, (n + 1) / 2); }, u"65% of your values"},
}};

// A trade stat's text as one of our stat keys.
QString keyForm(const QString &text)
{
    static const QRegularExpression local(uR"(\s*\(local\)\s*$)"_s,
                                          QRegularExpression::CaseInsensitiveOption);
    QString key = text;
    if (key.startsWith(u'+'))
        key.remove(0, 1);
    return key.remove(local).trimmed();
}

// "[Base] Energy Shield" -> "Energy Shield"
QString propertyName(const QString &key)
{
    const qsizetype end = key.indexOf(u"] ");
    return end < 0 ? key : key.mid(end + 2);
}

void appendOnce(QStringList &list, const QString &value)
{
    if (!list.contains(value))
        list << value;
}

} // namespace

QString listingStatusId(ListingStatus status)
{
    switch (status) {
    case ListingStatus::Available:
        break;
    case ListingStatus::Securable:
        return u"securable"_s;
    case ListingStatus::Online:
        return u"online"_s;
    case ListingStatus::Any:
        return u"any"_s;
    }
    return u"available"_s;
}

ListingStatus listingStatusFromId(QStringView id)
{
    if (id == u"securable")
        return ListingStatus::Securable;
    if (id == u"online")
        return ListingStatus::Online;
    if (id == u"any")
        return ListingStatus::Any;
    return ListingStatus::Available;
}

TradeQuery customSearch(const QList<StatPick> &picks, const TradeData &data,
                        const CustomSearchOptions &options)
{
    TradeQuery result;
    QJsonArray statFilters;
    QJsonObject equipment;
    for (const StatPick &pick : picks) {
        std::optional<StatMatch> m;
        if (!pick.id.isEmpty()) {
            m = StatMatch{StatMatch::Kind::Stat, pick.id, {}, pick.idType};
        } else {
            m = data.stats.match(pick.key, {.preferLocal = options.preferLocal});
            // the same kind of mod as on your item, unless a pseudo total covers them all
            if (m && m->kind == StatMatch::Kind::Stat && m->type != u"pseudo" && pick.origin) {
                const MatchOptions sameKind{.preferLocal = options.preferLocal,
                                            .type = modTypeFor(*pick.origin)};
                if (auto typed = data.stats.match(pick.key, sameKind))
                    m = typed;
            }
        }
        if (!m) {
            result.unmatched << pick.key;
            continue;
        }
        if (m->kind == StatMatch::Kind::Equipment)
            equipment.insert(m->id, minMax(pick.min, pick.max));
        else
            statFilters.append(statFilter(m->id, minMax(pick.min, pick.max)));
    }

    QJsonObject filters;
    if (!options.matchBase)
        addCategory(filters, data, options.itemClass);
    if (!equipment.isEmpty())
        filters.insert(u"equipment_filters"_s, filterGroup(equipment));

    QJsonObject query = newQuery(options.status);
    QJsonArray stats;
    if (!statFilters.isEmpty())
        stats.append(statGroup(u"and"_s, statFilters));
    query.insert(u"stats"_s, stats);
    query.insert(u"filters"_s, filters);
    if (options.matchBase && !options.baseType.isEmpty())
        query.insert(u"type"_s, options.baseType);
    result.excluded = addExcludeGroup(query, options.ignoreRules, data.stats);
    result.body = searchBody(query, u"price"_s, u"asc"_s);
    return result;
}

WeightedSearch weightedSearch(const Item &item, const Weights &weights, const TradeData &data,
                              const WeightedSearchOptions &options)
{
    // Listings mostly come without runes, so yours don't count towards the sum.
    QHash<QString, double> natural;
    for (const StatSource &x : statSources(item)) {
        if (x.origin != StatOrigin::Socket)
            natural[x.key] += x.value;
    }

    WeightedSearch result;
    QJsonArray weighted;
    QJsonObject equipment;
    // sorted, so the same weights always make the same search
    QStringList keys = weights.keys();
    std::sort(keys.begin(), keys.end());
    for (const QString &key : std::as_const(keys)) {
        const double w = weights.value(key);
        if (!std::isfinite(w) || w == 0 || isIgnored(options.ignoreRules, key))
            continue;
        if (const QString filter = equipmentFilter(key); !filter.isEmpty()) {
            const double own = item.stats.value(key);
            if (key.startsWith(u"[Weapon]") && own != 0 && w > 0)
                equipment.insert(filter, QJsonObject{{u"min"_s, std::floor(own)}});
            continue;
        }
        const auto m = data.stats.match(key, {.preferLocal = options.preferLocal});
        if (!m) {
            if (item.stats.value(key) != 0)
                result.unmatched << key;
            continue;
        }
        weighted.append(statFilter(m->id, {{u"weight"_s, w}}));
        result.current += natural.value(key) * w;
    }

    QJsonObject filters;
    addCategory(filters, data, options.itemClass.isEmpty() ? item.itemClass : options.itemClass);
    if (!equipment.isEmpty())
        filters.insert(u"equipment_filters"_s, filterGroup(equipment));

    result.groupMin = std::floor(result.current * (1 + options.minGainPercent / 100)) + 1;
    QJsonObject query = newQuery(options.status);
    QJsonArray stats;
    if (!weighted.isEmpty())
        stats.append(statGroup(u"weight"_s, weighted, {{u"min"_s, result.groupMin}}));
    query.insert(u"stats"_s, stats);
    query.insert(u"filters"_s, filters);
    result.excluded = addExcludeGroup(query, options.ignoreRules, data.stats);
    result.body = searchBody(query, u"statgroup.0"_s, u"desc"_s);
    return result;
}

int similarSearchLevels()
{
    return int(looseness.size());
}

SimilarSearch similarSearch(const Item &item, const TradeData &data, const SimilarSearchOptions &options)
{
    // Weapon properties other than total DPS say too little on their own, and
    // ward and block come with the base type.
    static const QRegularExpression skippedProperties(
        u"Attacks per Second|Critical Hit Chance|Physical DPS|Elemental DPS|Chaos DPS|Runic Ward|Block"_s);

    SimilarSearch result;
    result.level = std::clamp(options.level, 0, similarSearchLevels() - 1);
    const Looseness &loose = looseness[result.level];
    result.levelLabel = QString::fromUtf16(loose.label);
    result.unique = item.rarity.contains(u"unique", Qt::CaseInsensitive);

    struct Wanted
    {
        QString id;
        double value = 0;
    };
    QList<Wanted> wanted;
    QList<Wanted> pseudoTotals; // pseudo stats add up implicit and explicit mods, so they come last
    const auto addTo = [](QList<Wanted> &list, const QString &id, double value) {
        const auto it = std::find_if(list.begin(), list.end(), [&id](const Wanted &w) { return w.id == id; });
        if (it == list.end())
            list.append({id, value});
        else
            it->value += value;
    };

    QJsonObject equipment;
    const QList<StatSource> sources = statSources(item);
    for (const StatSource &x : sources) {
        if (isIgnored(options.ignoreRules, x.key)) {
            appendOnce(result.ignored, x.key);
            continue;
        }
        if (x.origin == StatOrigin::Base) {
            const QString filter = equipmentFilter(x.key);
            if (filter.isEmpty() || result.unique || skippedProperties.match(x.key).hasMatch())
                continue;
            if (const double v = baseWithoutSockets(x.key, x.value, sources); v > 0)
                equipment.insert(filter, QJsonObject{{u"min"_s, std::floor(v * 0.9 * loose.share)}});
            continue;
        }
        if (x.origin == StatOrigin::Socket) {
            result.skipped << x.key;
            continue;
        }
        const auto m = data.stats.match(x.key, {.preferLocal = options.preferLocal});
        if (!m) {
            appendOnce(result.unmatched, x.key);
            continue;
        }
        if (m->type == u"pseudo") {
            addTo(pseudoTotals, m->id, x.value);
            continue;
        }
        const QString type = x.origin == StatOrigin::Implicit ? u"implicit"_s : u"explicit"_s;
        const auto typed = data.stats.match(x.key, {.preferLocal = options.preferLocal, .type = type});
        addTo(wanted, typed ? typed->id : m->id, x.value);
    }
    wanted << pseudoTotals;

    QJsonArray statFilters;
    for (const Wanted &w : std::as_const(wanted)) {
        QJsonObject value;
        if (w.value > 0) {
            double min = roundTo(w.value >= 3 ? std::floor(w.value * loose.share) : w.value * loose.share, 2);
            // "+1 to Level of all Skills" mustn't round down to nothing
            if (w.value >= 1 && min < 1)
                min = 1;
            value.insert(u"min"_s, min);
        } else if (w.value < 0) {
            value.insert(u"max"_s, roundTo(w.value * loose.share, 2));
        }
        statFilters.append(statFilter(w.id, value));
    }

    result.total = int(statFilters.size());
    result.need = std::max(1, result.total - loose.mayMiss(result.total));
    // the last try for a unique is just its name
    if (result.unique && result.level == similarSearchLevels() - 1)
        result.need = 0;

    QJsonObject filters;
    if (!result.unique)
        addCategory(filters, data, item.itemClass);
    if (!equipment.isEmpty())
        filters.insert(u"equipment_filters"_s, filterGroup(equipment));

    QJsonObject query = newQuery(options.status);
    QJsonArray stats;
    if (result.total > 0 && result.need > 0)
        stats.append(statGroup(u"count"_s, statFilters, {{u"min"_s, result.need}}));
    query.insert(u"stats"_s, stats);
    query.insert(u"filters"_s, filters);
    if (result.unique) {
        if (!item.name.isEmpty())
            query.insert(u"name"_s, item.name);
        if (!item.baseType.isEmpty() && item.baseType != item.name)
            query.insert(u"type"_s, item.baseType);
    }
    result.excluded = addExcludeGroup(query, options.ignoreRules, data.stats);
    result.body = searchBody(query, u"price"_s, u"asc"_s);
    return result;
}

CraftBaseSearch craftBaseSearch(const Item &item, const QList<KeyStat> &keyStats, const TradeData &data,
                                const SearchOptions &options)
{
    CraftBaseSearch result;
    QJsonArray wanted;
    for (const KeyStat &k : keyStats) {
        result.keyStats << k.label;
        const auto m = data.stats.match(k.key, {.preferLocal = options.preferLocal});
        if (!m || m->kind != StatMatch::Kind::Stat) {
            result.unmatched << k.key;
            continue;
        }
        const double min = k.cap >= 5 ? std::floor(k.cap * 0.55) : std::max(1.0, std::floor(k.cap * 0.5));
        wanted.append(statFilter(m->id, {{u"min"_s, min}}));
    }
    QJsonArray openAffix;
    for (const QString &key : {u"# Empty Prefix Modifiers"_s, u"# Empty Suffix Modifiers"_s}) {
        if (const auto m = data.stats.match(key); m && m->kind == StatMatch::Kind::Stat)
            openAffix.append(statFilter(m->id, {{u"min"_s, 1}}));
    }
    result.emptyAffixFilter = !openAffix.isEmpty();

    QJsonObject typeFilters{{u"rarity"_s, option(u"rare"_s)}};
    if (const QString category = data.categories.categoryFor(item.itemClass); !category.isEmpty())
        typeFilters.insert(u"category"_s, option(category));
    QJsonObject filters{
        {u"type_filters"_s, filterGroup(typeFilters)},
        {u"misc_filters"_s, filterGroup({{u"corrupted"_s, option(u"false"_s)}})},
    };
    // a base at least about as good as yours, without your runes
    QJsonObject equipment;
    const QList<StatSource> sources = statSources(item);
    static const QRegularExpression notABase(u"Weapon|Runic|Block"_s);
    for (const StatSource &x : sources) {
        const QString filter = equipmentFilter(x.key);
        if (x.origin != StatOrigin::Base || filter.isEmpty() || notABase.match(x.key).hasMatch())
            continue;
        if (const double v = baseWithoutSockets(x.key, x.value, sources); v > 0)
            equipment.insert(filter, QJsonObject{{u"min"_s, std::floor(v * 0.9)}});
    }
    if (!equipment.isEmpty())
        filters.insert(u"equipment_filters"_s, filterGroup(equipment));

    QJsonObject query = newQuery(options.status);
    QJsonArray stats;
    if (!wanted.isEmpty())
        stats.append(statGroup(u"and"_s, wanted));
    if (!openAffix.isEmpty())
        stats.append(statGroup(u"count"_s, openAffix, {{u"min"_s, 1}}));
    query.insert(u"stats"_s, stats);
    query.insert(u"filters"_s, filters);
    result.excluded = addExcludeGroup(query, options.ignoreRules, data.stats);
    result.body = searchBody(query, u"price"_s, u"asc"_s);
    return result;
}

GearSearch gearSearch(const Item &item, const QList<SearchWeight> &weights, const TradeData &data,
                      const GearSearchOptions &options)
{
    // Key stats need their plain version; the conditional ones only count for less.
    static const QRegularExpression conditional(uR"(\b(if you've|while|when|recently)\b)"_s,
                                                QRegularExpression::CaseInsensitiveOption);
    const double baseShare = options.baseShare > 0 ? options.baseShare : 0.8;
    const qsizetype maxFilters = options.maxFilters > 0 ? options.maxFilters : 40;
    // trade weights are easier to read as whole numbers
    constexpr double scale = 100;

    struct Weighted
    {
        QString id;
        double weight = 0;
    };
    struct Key
    {
        const SearchWeight *weight = nullptr;
        QList<const TradeStat *> ids;
    };

    GearSearch result;
    const QList<StatSource> sources = statSources(item);
    QList<Weighted> weighted;
    QSet<QString> used;
    QList<Key> core;
    QJsonObject equipment;
    for (const SearchWeight &w : weights) {
        // [Base] and [Weapon] stats aren't mods: yours become a minimum
        bool isProperty = w.pattern.pattern().startsWith(u"^\\[");
        for (const QString &key : equipmentKeys()) {
            if (!w.pattern.match(key).hasMatch())
                continue;
            isProperty = true;
            double own = 0;
            for (const StatSource &x : sources) {
                if (x.key == key)
                    own += x.value;
            }
            if (own <= 0)
                continue;
            const double min = std::floor(baseWithoutSockets(key, own, sources) * baseShare);
            equipment.insert(equipmentFilter(key), QJsonObject{{u"min"_s, min}});
            result.parts << propertyName(key) + u" at least "_s + formatNumber(min);
        }
        if (isProperty)
            continue;

        // every trade stat the weight covers, as minion crit has several
        QList<const TradeStat *> ids;
        for (const TradeStat &e : data.stats.all()) {
            const bool wantedType = e.type == u"explicit" || (!options.explicitOnly && e.type == u"implicit");
            if (wantedType && w.pattern.match(keyForm(e.text)).hasMatch())
                ids << &e;
        }
        if (ids.isEmpty())
            continue;
        const double ww = roundTo(w.weight * scale, 2);
        for (const TradeStat *e : std::as_const(ids)) {
            if (used.contains(e->id))
                continue;
            used.insert(e->id);
            // conditional mods count for less in the gear score, so here too
            weighted.append({e->id, roundTo(ww * (isConditionalStat(e->text) ? 0.6 : 1), 2)});
        }
        if (w.importance >= 4 && !w.key.isEmpty()) {
            Key k{&w, {}};
            for (const TradeStat *e : std::as_const(ids)) {
                if (!conditional.match(e->text).hasMatch())
                    k.ids << e;
            }
            core << k;
        }

        // what your item scores on it, leaving your runes out like the other searches
        double own = 0;
        for (const StatSource &x : sources) {
            if (x.origin != StatOrigin::Socket && w.pattern.match(x.key).hasMatch())
                own += x.value * (isConditionalStat(x.key) ? 0.6 : 1);
        }
        if (w.resist && w.need)
            own = std::min(own, std::max(0.0, *w.need));
        result.current += own * ww;
        if (own > 0)
            result.parts << w.label;
    }

    // the trade site limits how many filters a group can have, so keep the heaviest
    std::stable_sort(weighted.begin(), weighted.end(),
                     [](const Weighted &a, const Weighted &b) { return a.weight > b.weight; });
    const QList<Weighted> kept = weighted.mid(0, maxFilters);
    result.filters = int(kept.size());
    result.dropped = int(weighted.size() - kept.size());

    QJsonObject filters;
    addCategory(filters, data, options.itemClass.isEmpty() ? item.itemClass : options.itemClass);
    if (!equipment.isEmpty())
        filters.insert(u"equipment_filters"_s, filterGroup(equipment));

    result.groupMin =
        std::max(1.0, std::floor(result.current * options.share) + (options.share >= 1 ? 1 : 0));
    QJsonArray stats;
    if (!kept.isEmpty()) {
        QJsonArray group;
        for (const Weighted &w : kept)
            group.append(statFilter(w.id, {{u"weight"_s, w.weight}}));
        stats.append(statGroup(u"weight"_s, group, {{u"min"_s, result.groupMin}}));
    }

    // the slot's key stats: a listing has to have as many as your item does, up to two
    std::stable_sort(core.begin(), core.end(),
                     [](const Key &a, const Key &b) { return a.weight->importance > b.weight->importance; });
    QList<Key> keys;
    for (const Key &k : core.mid(0, 5)) {
        if (!k.ids.isEmpty())
            keys << k;
    }
    int ownKeys = 0;
    for (const Key &k : std::as_const(keys)) {
        result.keyStats << k.weight->label;
        const bool has = std::any_of(sources.cbegin(), sources.cend(), [&k](const StatSource &x) {
            return x.origin != StatOrigin::Socket && x.value > 0 && k.weight->pattern.match(x.key).hasMatch();
        });
        ownKeys += has ? 1 : 0;
    }
    result.needKeyStats = std::min(2, ownKeys);
    if (!kept.isEmpty() && result.needKeyStats > 0 && options.keyStatShare > 0) {
        QJsonArray group;
        for (const Key &k : std::as_const(keys)) {
            const double min = std::max(1.0, std::floor(k.weight->cap * options.keyStatShare));
            for (const TradeStat *e : k.ids)
                group.append(statFilter(e->id, {{u"min"_s, min}}));
        }
        stats.append(statGroup(u"count"_s, group, {{u"min"_s, result.needKeyStats}}));
    }

    // The smallest form: "has some of your key stats at a decent roll". The
    // results window still rates each listing with your gear score.
    result.countOnly = options.countOnly && !keys.isEmpty();
    if (result.countOnly) {
        QJsonArray group;
        for (const Key &k : std::as_const(keys)) {
            const auto e = std::find_if(k.ids.cbegin(), k.ids.cend(),
                                        [](const TradeStat *s) { return s->type == u"explicit"; });
            const TradeStat *id = e != k.ids.cend() ? *e : k.ids.first();
            group.append(statFilter(id->id, {{u"min"_s, std::max(1.0, std::floor(k.weight->cap * 0.5))}}));
        }
        result.needKeyStats = std::max(1, std::min({3, ownKeys > 0 ? ownKeys : 2, int(group.size())}));
        stats = {statGroup(u"count"_s, group, {{u"min"_s, result.needKeyStats}})};
    }

    QJsonObject query = newQuery(options.status);
    query.insert(u"stats"_s, stats);
    query.insert(u"filters"_s, filters);
    if (!options.countOnly)
        result.excluded = addExcludeGroup(query, options.ignoreRules, data.stats);
    // best for your build first; the results window can sort by price
    result.body = result.countOnly ? searchBody(query, u"price"_s, u"asc"_s)
                                   : searchBody(query, u"statgroup.0"_s, u"desc"_s);
    return result;
}

} // namespace pou
