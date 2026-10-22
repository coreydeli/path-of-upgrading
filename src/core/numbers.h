// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace pou {

// Rounds halves towards +infinity: 2.5 -> 3, -2.5 -> -2.
double roundHalfUp(double v);

// roundHalfUp() at a number of decimals: roundTo(1.25, 1) == 1.3.
double roundTo(double v, int decimals);

// Shortest text that reads back as the same number, without exponents for
// everyday magnitudes: 20, 1.5, 0.1, -3, 0.000001.
QString formatNumber(double v);

} // namespace pou
