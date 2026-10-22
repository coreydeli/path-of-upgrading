// SPDX-License-Identifier: MIT
#include "core/tradedata.h"

#include "core/statnorm.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>
#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

const QList<std::pair<QString, QString>> &equipmentFilters()
{
    static const QList<std::pair<QString, QString>> filters = {
        {u"[Base] Armour"_s, u"ar"_s},
        {u"[Base] Evasion Rating"_s, u"ev"_s},
        {u"[Base] Energy Shield"_s, u"es"_s},
        {u"[Base] Block Chance"_s, u"block"_s},
        {u"[Base] Spirit"_s, u"spirit"_s},
        {u"[Base] Runic Ward"_s, u"ward"_s},
        {u"[Weapon] Total DPS"_s, u"dps"_s},
        {u"[Weapon] Physical DPS"_s, u"pdps"_s},
        {u"[Weapon] Elemental DPS"_s, u"edps"_s},
        {u"[Weapon] Attacks per Second"_s, u"aps"_s},
        {u"[Weapon] Critical Hit Chance"_s, u"crit"_s},
    };
    return filters;
}

// Lower is better when several trade stats share a text.
int typeRank(const QString &type)
{
    static const QStringList order = {
        u"pseudo"_s,  u"explicit"_s, u"implicit"_s,   u"augment"_s,   u"rune"_s,
        u"enchant"_s, u"crafted"_s,  u"desecrated"_s, u"fractured"_s,
    };
    const qsizetype i = order.indexOf(type);
    return i < 0 ? 99 : int(i);
}

// "Helmets" -> "helmet", the way the category options are worded.
QString singular(const QString &itemClass)
{
    QString c = itemClass.trimmed().toLower();
    if (c == u"foci")
        return u"focus"_s;
    if (c.endsWith(u"staves"))
        return c.chopped(6) + u"staff";
    if (c.endsWith(u"ies"))
        return c.chopped(3) + u'y';
    if (c.endsWith(u's'))
        c.chop(1);
    return c;
}

} // namespace

QString equipmentFilter(QStringView key)
{
    for (const auto &[stat, filter] : equipmentFilters()) {
        if (stat == key)
            return filter;
    }
    return {};
}

const QStringList &equipmentKeys()
{
    static const QStringList keys = [] {
        QStringList out;
        for (const auto &[stat, filter] : equipmentFilters())
            out << stat;
        return out;
    }();
    return keys;
}

StatIndex StatIndex::fromJson(const QJsonObject &response)
{
    static const QRegularExpression local(uR"(\(local\))"_s, QRegularExpression::CaseInsensitiveOption);

    StatIndex index;
    for (const QJsonValue &group : response.value(u"result").toArray()) {
        const QString groupId = group[u"id"].toString();
        for (const QJsonValue &entry : group[u"entries"].toArray()) {
            TradeStat stat;
            stat.id = entry[u"id"].toString();
            stat.text = entry[u"text"].toString();
            stat.type = entry[u"type"].toString();
            if (stat.type.isEmpty())
                stat.type = groupId;
            stat.local = local.match(stat.text).hasMatch();
            if (stat.id.isEmpty() || stat.text.isEmpty())
                continue;
            index.m_byText[normalizeStat(stat.text)].append(index.m_stats.size());
            index.m_stats.append(std::move(stat));
        }
    }
    return index;
}

std::optional<StatMatch> StatIndex::match(const QString &key, const MatchOptions &options) const
{
    if (const QString filter = equipmentFilter(key); !filter.isEmpty())
        return StatMatch{StatMatch::Kind::Equipment, filter, key, {}};

    QList<const TradeStat *> candidates;
    for (qsizetype i : m_byText.value(normalizeStat(key)))
        candidates << &m_stats[i];
    if (!options.type.isEmpty()) {
        const auto ofType = [&](const QString &type) {
            QList<const TradeStat *> out;
            std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(out),
                         [&](const TradeStat *s) { return s->type == type; });
            return out;
        };
        const QList<const TradeStat *> same = ofType(options.type);
        candidates = same.isEmpty() ? ofType(u"explicit"_s) : same;
    }
    if (candidates.isEmpty())
        return std::nullopt;

    std::stable_sort(candidates.begin(), candidates.end(), [&](const TradeStat *a, const TradeStat *b) {
        if (a->local != b->local)
            return a->local == options.preferLocal;
        return typeRank(a->type) < typeRank(b->type);
    });
    const TradeStat *best = candidates.first();
    return StatMatch{StatMatch::Kind::Stat, best->id, best->text, best->type};
}

TradeCategories TradeCategories::fromJson(const QJsonObject &response)
{
    TradeCategories categories;
    for (const QJsonValue &group : response.value(u"result").toArray()) {
        for (const QJsonValue &filter : group[u"filters"].toArray()) {
            if (filter[u"id"].toString() != u"category")
                continue;
            for (const QJsonValue &option : filter[u"option"][u"options"].toArray()) {
                const QString id = option[u"id"].toString();
                const QString name = option[u"text"].toString().toLower();
                if (!id.isEmpty() && !categories.m_byName.contains(name))
                    categories.m_byName.insert(name, id);
            }
        }
    }
    return categories;
}

QString TradeCategories::categoryFor(const QString &itemClass) const
{
    static const QHash<QString, QString> builtIn = {
        {u"helmets"_s, u"armour.helmet"_s},
        {u"body armours"_s, u"armour.chest"_s},
        {u"gloves"_s, u"armour.gloves"_s},
        {u"boots"_s, u"armour.boots"_s},
        {u"shields"_s, u"armour.shield"_s},
        {u"foci"_s, u"armour.focus"_s},
        {u"bucklers"_s, u"armour.buckler"_s},
        {u"quivers"_s, u"armour.quiver"_s},
        {u"rings"_s, u"accessory.ring"_s},
        {u"amulets"_s, u"accessory.amulet"_s},
        {u"belts"_s, u"accessory.belt"_s},
        {u"jewels"_s, u"jewel"_s},
        {u"charms"_s, u"flask.charm"_s},
        {u"life flasks"_s, u"flask.life"_s},
        {u"mana flasks"_s, u"flask.mana"_s},
        {u"bows"_s, u"weapon.bow"_s},
        {u"crossbows"_s, u"weapon.crossbow"_s},
        {u"talismans"_s, u"weapon.talisman"_s},
        {u"quarterstaves"_s, u"weapon.warstaff"_s},
        {u"staves"_s, u"weapon.staff"_s},
        {u"two hand swords"_s, u"weapon.twosword"_s},
        {u"two hand axes"_s, u"weapon.twoaxe"_s},
        {u"two hand maces"_s, u"weapon.twomace"_s},
        {u"one hand swords"_s, u"weapon.onesword"_s},
        {u"spears"_s, u"weapon.spear"_s},
        {u"flails"_s, u"weapon.flail"_s},
        {u"one hand axes"_s, u"weapon.oneaxe"_s},
        {u"one hand maces"_s, u"weapon.onemace"_s},
        {u"sceptres"_s, u"weapon.sceptre"_s},
        {u"wands"_s, u"weapon.wand"_s},
        {u"daggers"_s, u"weapon.dagger"_s},
        {u"claws"_s, u"weapon.claw"_s},
    };

    if (const QString id = m_byName.value(singular(itemClass)); !id.isEmpty())
        return id;
    return builtIn.value(itemClass.trimmed().toLower());
}

} // namespace pou
