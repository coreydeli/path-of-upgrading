// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"
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

// "Life 60 or more (up to 149), Cold res 25 or more".
QString wantedText(const QList<WantedStat> &stats);

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
