// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "core/slotmods.h"

#include <QString>
#include <optional>

namespace pou {

// Where a rolled value sits among the tiers of a mod, tier 1 being the best.
struct StatTier
{
    int number = 0;                // 1 is the top tier
    int count = 0;                 // how many tiers the mod has on that class
    ModTier range;                 // this tier
    std::optional<ModTier> better; // the next tier up, empty at tier 1
    int betterNumber = 0;

    friend bool operator==(const StatTier &, const StatTier &) = default;
};

// The tier a value was rolled in, for an item class and stat key. Empty when
// the table has nothing for the stat, when the value is above the top tier or
// between two, and when it could be more than one mod: tiers that overlap, or
// the range of a hybrid mod that gives the same stat. Only for a value one mod
// gave, not a sum.
std::optional<StatTier> tierOf(const SlotMods &mods, const QString &itemClass, const QString &key,
                               double value);

// The tier of a stat the item has from one explicit mod, as the class rolls
// it. Empty when the stat also comes from another mod (two explicit ones, or
// "all attributes"), when it's only an implicit, a rune or an enchant, and when
// the item has no stat lines, as a plain copy gives no mod to name.
std::optional<StatTier> rolledTier(const SlotMods &mods, const Item &item, const QString &itemClass,
                                   const QString &key);

// The tier a search has to ask for to get at least `minimum`: the lowest tier
// that can still roll it. Empty when the table has nothing for the stat or no
// tier reaches that high.
std::optional<int> tierFor(const SlotMods &mods, const QString &itemClass, const QString &key,
                           double minimum);

// "tier 4 of 10"
QString tierText(const StatTier &tier);
// The next tier up: "tier 3 (120 to 149) at item level 60 or higher". Empty at tier 1.
QString nextTierText(const StatTier &tier);

} // namespace pou
