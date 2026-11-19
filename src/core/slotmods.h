// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

namespace pou {

// One tier of a mod: the lowest item level it can come on, and what it rolls.
// A hybrid mod gives more than one stat, and its ranges are its own.
struct ModTier
{
    int level = 0;
    double low = 0;
    double high = 0;
    bool hybrid = false;

    friend bool operator==(const ModTier &, const ModTier &) = default;
};

// What each kind of item can roll, by item class ("Boots", "One Hand Swords")
// and stat key ("# to maximum Life"). The table is made from Path of
// Building's data by tools/slot-mods.py, and read from the resource it's
// kept in.
class SlotMods
{
public:
    SlotMods() = default;
    // The table the app ships with.
    static const SlotMods &builtIn();
    static SlotMods fromJson(const QJsonObject &table);

    // Highest tier first. Empty when the class can't roll the stat, or the
    // class isn't in the table. Keys go by case, as the game's text has
    // "damage" and "Damage" for the same stat.
    QList<ModTier> tiers(const QString &itemClass, const QString &key) const;
    // Whether a class rolls the stat as a prefix or a suffix. Unknown when the
    // class or stat isn't in the table, or when the class has it as both.
    AffixKind kind(const QString &itemClass, const QString &key) const;
    // False when the table has the class and the class can't roll the stat as
    // a prefix or suffix. A class the table doesn't have (the weapon slot,
    // jewels) is allowed anything, as nothing says otherwise.
    bool allows(const QString &itemClass, const QString &key) const;
    // The best tier, or the best an item of that level can come with. A hybrid
    // mod's tiers only count for a stat that has no other kind.
    std::optional<ModTier> best(const QString &itemClass, const QString &key) const;
    std::optional<ModTier> bestAt(const QString &itemClass, const QString &key, int itemLevel) const;

    // Where the table is from, as it says: "0.23.1" and "2026-11-10".
    QString version() const { return m_version; }
    QString date() const { return m_date; }
    bool isEmpty() const { return m_classes.isEmpty(); }

private:
    // lower case class, then lower case key
    QHash<QString, QHash<QString, QList<ModTier>>> m_classes;
    QHash<QString, QHash<QString, AffixKind>> m_kinds;
    QString m_version;
    QString m_date;
};

// The item class a slot holds, when it holds only one: "Helmet" is
// "Helmets". Empty for the weapon, the offhand, jewels, flasks and charms.
QString classOfSlot(const QString &slot);

// A roll in a note: 149, 35.5.
QString rollText(double value);

} // namespace pou
