// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace pou {

// One normal form for stat texts, so "+# to maximum Life" (trade explicit),
// "+# total maximum Life" (trade pseudo) and "# to maximum Life" (our item
// key) all compare equal.
QString normalizeStat(const QString &text);

// Mods that only work some of the time: "if you've Hit Recently", "while on
// Low Life", "per Power Charge". Not "per second", which is always on.
bool isConditionalStat(const QString &text);

} // namespace pou
