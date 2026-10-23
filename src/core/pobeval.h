// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "core/pobverdict.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace pou {

// Path of Building's answer when it's asked to judge items, see eval in
// pobbridge.lua.

// One of the places it tried an item in, with the build's numbers when the
// item is there.
struct PobPlacement
{
    QString slot;      // its name for the place: "Ring 2", "Weapon 1 Swap", "Jewel 61834"...
    QString label;     // what it shows for it: "Ring 2", "Socket #3"...
    QString replacing; // its name for the item there, empty when there's none
    PobStats stats;
};

// The places it tried one item in, or why it couldn't.
struct PobItemEval
{
    QList<PobPlacement> placements;
    QString error;
};

struct PobEval
{
    PobStats before;          // the build as it is
    QList<PobItemEval> items; // in the order they were asked about
};

// The numbers in one of its answers, leaving out what isn't a number.
PobStats readPobStats(const QJsonObject &stats);

PobEval readPobEval(const QJsonObject &answer);

// Its name for an item, the way placements name what they replace:
// "Grim Grasp, Prismatic Ring" for rares and uniques.
QString pobItemName(const Item &item);

} // namespace pou
