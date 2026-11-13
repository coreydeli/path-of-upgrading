// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterdata.h"
#include "core/gearscore.h"
#include "core/pobeval.h"

#include <QList>
#include <QString>
#include <optional>

namespace pou {

// Which slots to look for an upgrade in, and what to look for there.

// A stat to look for in a slot, with the least of it worth having.
struct WantedStat
{
    QString label; // "Life"
    double minimum = 0;
    double best = 0; // the most the slot's items roll of it, 0 when that's not known

    friend bool operator==(const WantedStat &, const WantedStat &) = default;
};

// The build's most important stats for a slot, as many as `count`, with what
// a search for a better item would ask of each (as a craft base search does).
// Resistances are there while you still need them, and no more than you need.
// A stat the worn item has that much of already is left out, as are those you
// ignore. `worn` is null for an empty slot.
QList<WantedStat> wantedStats(const QString &slot, const Item *worn, const GearScoreOptions &options,
                              int count = 3);

// "Life 60 or more (up to 149), Cold Resistance 25 or more".
QString wantedText(const QList<WantedStat> &stats);

// What a slot can roll of a stat the build wants: the top tier's range, and the
// item level it needs.
struct RollRange
{
    QString label; // "Life"
    ModTier top;

    friend bool operator==(const RollRange &, const RollRange &) = default;
};

// The stats the build wants in a slot, as many as `count`, with what their top
// tier rolls on the item class there (the worn item's, else the slot's).
// Those the table has nothing for are left out, so it's empty for a slot
// with no class, like the weapon. Unlike wantedStats(), it doesn't matter what
// is worn.
QList<RollRange> canRoll(const QString &slot, const Item *worn, const GearScoreOptions &options,
                         int count = 6);

// "Life 100 to 149, item level 82", or one number when the tier has no range.
QString rollRangeText(const RollRange &range);

struct Recommendation
{
    QString slot;
    std::optional<int> score; // of what's worn, empty when nothing is
    QString grade;
    QList<WantedStat> wants;
};

// Up to `count` of the main slots to upgrade first: those with nothing in
// them, then the worn ones with the lowest gear scores. A slot with nothing to
// look for is left out, and so is a worn item that has no gear score (a
// unique) and the offhand while a two-handed weapon is worn.
QList<Recommendation> recommendedUpgrades(const Character &character, const PobPriorities &priorities = {},
                                          int count = 3);

} // namespace pou
