// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "core/scorer.h"

#include <QList>
#include <QString>

namespace pou {

// A weight to try for a stat your gear has.
struct SuggestedWeight
{
    QString key;
    double weight = 0;  // to two significant digits
    int items = 0;      // how many of your items have it
    double average = 0; // its average roll on those, to one decimal
};

// Weights from the gear you wear: the stats your build already stacks are
// the ones it cares about. A typical roll of each is worth about 30 points,
// more when several of your items have it. Uniques are left out, as their
// mods are fixed, and so are runes, which go on anything.
//
// Stats on the most items come first.
QList<SuggestedWeight> suggestWeights(const Equipped &equipped, const StatFilter &ignore = {});

} // namespace pou
