// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace pou {

// One normal form for stat texts, so "+# to maximum Life" (trade explicit),
// "+# total maximum Life" (trade pseudo) and "# to maximum Life" (our item
// key) all compare equal.
QString normalizeStat(const QString &text);

} // namespace pou
