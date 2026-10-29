// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

namespace pou {

// One normal form for stat texts, so "+# to maximum Life" (trade explicit),
// "+# total maximum Life" (trade pseudo) and "# to maximum Life" (our item
// key) all compare equal.
QString normalizeStat(const QString &text);

// Mods that only work some of the time: "if you've Hit Recently", "while on
// Low Life", "per Power Charge". Not "per second", which is always on.
bool isConditionalStat(const QString &text);

// The item's own, like "[Base] Armour" or "[Weapon] Total DPS", rather than
// one of its mods.
bool isItemProperty(QStringView key);

// What a stat is called in the window: "Armour" for "[Base] Armour", and the
// key as it is for a mod.
QString statName(const QString &key);
// What the window says of one of the item's own, and nothing for a mod.
QString propertyNote(QStringView key);
// The item's own first, then by name.
void sortStats(QStringList &keys);

} // namespace pou
