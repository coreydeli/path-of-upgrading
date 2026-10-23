// SPDX-License-Identifier: MIT
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// One stat the trade site can filter on, from /api/trade2/data/stats.
struct TradeStat
{
    QString id;         // "explicit.stat_3299347043"
    QString text;       // "+# to maximum Life"
    QString type;       // pseudo, explicit, implicit, rune, enchant...
    bool local = false; // "(Local)": only applies to the item it's on
};

struct StatMatch
{
    enum class Kind { Stat, Equipment };

    Kind kind = Kind::Stat;
    QString id;
    QString text;
    QString type;
};

struct MatchOptions
{
    // For armour and weapons, which have their own "(Local)" versions of
    // defence and damage mods.
    bool preferLocal = false;
    // Only stats of this type, or explicit ones if it has none. Empty means
    // any type, with pseudo stats (which add up every version) first.
    QString type;
};

// The trade site's equipment filter id ("ar", "dps"...) for a [Base] or
// [Weapon] stat, or empty when there isn't one.
QString equipmentFilter(QStringView key);
// The stats that have an equipment filter, in a fixed order.
const QStringList &equipmentKeys();

class StatIndex
{
public:
    // From the /api/trade2/data/stats response.
    static StatIndex fromJson(const QJsonObject &response);

    bool isEmpty() const { return m_stats.isEmpty(); }
    const QList<TradeStat> &all() const { return m_stats; }

    // The trade stat that fits one of our stat keys best.
    std::optional<StatMatch> match(const QString &key, const MatchOptions &options = {}) const;

private:
    QList<TradeStat> m_stats;
    QHash<QString, QList<qsizetype>> m_byText; // normalizeStat(text) -> positions in m_stats
};

// Item category ids for the type filter, from /api/trade2/data/filters.
class TradeCategories
{
public:
    static TradeCategories fromJson(const QJsonObject &response);

    // Whether there are none from the site, only the ones built in.
    bool isEmpty() const { return m_byName.isEmpty(); }

    // "armour.helmet" for "Helmets". Falls back to ids built in for when the
    // filters couldn't be fetched, and is empty for classes without one.
    QString categoryFor(const QString &itemClass) const;

private:
    QHash<QString, QString> m_byName; // "helmet" -> "armour.helmet"
};

} // namespace pou
