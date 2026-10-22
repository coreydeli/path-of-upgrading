// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"

#include <optional>

namespace pou {

// Parses the text the game puts on the clipboard when you hover an item and
// press Ctrl+C (or Ctrl+Alt+C for the advanced version with mod tiers).
// Returns nothing when the text isn't an item.
std::optional<Item> parseItem(const QString &text);

// Mods that really are several stats at once, split up so a weight on Fire
// Resistance also counts "+10% to all Elemental Resistances".
QList<StatMap::Entry> expandStat(const QString &key, double value);

} // namespace pou
