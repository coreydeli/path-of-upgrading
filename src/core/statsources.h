// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"

#include <QList>
#include <QString>

namespace pou {

// Where a stat on an item comes from. Runes and enchants ("socket") can be
// put on any item by whoever buys it, so searches for similar items leave
// them out.
enum class StatOrigin { Mod, Implicit, Socket, Base };

struct StatSource
{
    QString key;
    double value = 0;
    StatOrigin origin = StatOrigin::Mod;
};

// The item's stats split by origin, summed per key and origin from its mod
// lines. Items without stat lines (built by hand, or saved without them) fall
// back to their stats, with [Base] and [Weapon] values as Base and everything
// else as Mod.
QList<StatSource> statSources(const Item &item);

// An armour piece's base Armour, Evasion, Energy Shield or Spirit without what
// its runes add. Runes count towards the "% increased" total, so the same base
// listed without them shows less.
double baseWithoutSockets(const QString &key, double value, const QList<StatSource> &sources);

// Local defence mods ("#% increased Armour", "+# to maximum Energy Shield") on
// armour, and "#% increased Spirit" on a sceptre, only raise the item's own
// base value, which its [Base] stat already counts.
bool isInBase(const Item &item, const QString &key);

} // namespace pou
