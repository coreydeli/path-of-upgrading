// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"

#include <QList>
#include <QString>

namespace pou {

// One mod of an item, under the heading the game puts it in.
struct WornMod
{
    QString group; // "Implicit", "Prefixes", "Suffixes", "Runes", "Enchants", "Other mods"
    QString text;  // as the game wrote it, without the "(fractured)" tag
    bool fractured = false;

    friend bool operator==(const WornMod &, const WornMod &) = default;
};

// The item's mods in the order the game shows them: implicit, prefixes,
// suffixes, runes, then enchants. A rare or magic mod that nothing says the
// kind of goes under "Other mods", and the mods of a unique are all "Mods".
QList<WornMod> wornMods(const Item &item);

// Any of its mods is fractured.
bool hasFracturedMod(const Item &item);

} // namespace pou
