// SPDX-License-Identifier: MIT
#include "core/tradequery.h"

#include <QJsonArray>

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
    if (!options.matchBase) {
        if (const QString category = data.categories.categoryFor(options.itemClass); !category.isEmpty())
            filters.insert(u"type_filters"_s, categoryFilter(category));
    }
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

} // namespace pou
