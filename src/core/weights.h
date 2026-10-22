// SPDX-License-Identifier: MIT
#pragma once

#include <QHash>
#include <QString>

namespace pou {

// Stat key -> what one point of that stat is worth to you.
using Weights = QHash<QString, double>;

// A starting point that suits most life and resistance characters.
const Weights &defaultWeights();

} // namespace pou
