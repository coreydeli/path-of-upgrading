// SPDX-License-Identifier: MIT
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

namespace pou {

// One tier of a mod: the lowest item level it can come on, and what it rolls.
struct ModTier
{
    int level = 0;
    double low = 0;
    double high = 0;

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
    // class isn't in the table.
    QList<ModTier> tiers(const QString &itemClass, const QString &key) const;
    // The best tier, or the best an item of that level can come with.
    std::optional<ModTier> best(const QString &itemClass, const QString &key) const;
    std::optional<ModTier> bestAt(const QString &itemClass, const QString &key, int itemLevel) const;

    // Where the table is from, as it says: "0.23.1" and "2026-11-10".
    QString version() const { return m_version; }
    QString date() const { return m_date; }
    bool isEmpty() const { return m_classes.isEmpty(); }

private:
    // lower case class, then key
    QHash<QString, QHash<QString, QList<ModTier>>> m_classes;
    QString m_version;
    QString m_date;
};

// The item class a slot holds, when it holds only one: "Helmet" is
// "Helmets". Empty for the weapon, the offhand, jewels, flasks and charms.
QString classOfSlot(const QString &slot);

// A roll in a note: 149, 35.5.
QString rollText(double value);

} // namespace pou
