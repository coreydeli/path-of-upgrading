// SPDX-License-Identifier: MIT
#pragma once

#include "core/statmap.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// One line of item text, as listed in the popup.
struct ItemLine
{
    QString text; // as the game wrote it
    QString key;  // stat template like "# to maximum Life", empty when the line isn't a stat
    double value = 0;
    // explicit, implicit, rune, enchant, crafted, fractured, desecrated, augmented,
    // unmet, veiled, mutated; "base" for base properties and "info" for other properties
    QString tag;
};

// Prefix and suffix counts. Only advanced copies (Ctrl+Alt+C) say which mod
// is which, so for plain copies this stays unknown.
struct Affixes
{
    bool known = false;
    int prefixes = 0;
    int suffixes = 0;
    int openPrefix = 0;
    int openSuffix = 0;
};

// Mageblood's Mage's Legacies, see parseItem().
struct Legacies
{
    QStringList names;
    int duplicates = 0;
    double effect = 1;
};

struct Item
{
    QString raw; // the game's own text, which is what Path of Building gets
    QString itemClass;
    QString rarity;
    QString name;
    QString baseType; // empty when the text doesn't say, as for magic items
    QString slot;     // slot group from slotForClass(), empty when it isn't gear
    bool twoHanded = false;
    int itemLevel = 0; // 0 when the text doesn't show one
    int requiredLevel = 0;
    bool corrupted = false;
    bool unidentified = false;
    StatMap stats; // what gets scored
    QList<ItemLine> lines;
    Affixes affixes;
    std::optional<Legacies> legacies;
};

} // namespace pou
