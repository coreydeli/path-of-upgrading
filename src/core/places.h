// SPDX-License-Identifier: MIT
#pragma once

#include "core/evaluation.h"
#include "core/pobeval.h"

#include <QList>
#include <QString>
#include <optional>

namespace pou {

// How an item does in each place it could go, like either ring slot, for the
// popup to put side by side.

struct PlaceChange
{
    QString slot;    // "Ring 2", or Path of Building's name for a jewel socket
    QString wearing; // what's there now, empty when nothing is
    // The place the verdict is for.
    bool best = false;
    ScoreMode mode = ScoreMode::GearScores;
    // How much better the item is there: gear score or weights points, or with
    // Path of Building its score in percent.
    double score = 0;
    // Only from Path of Building, in percent. Unknown when it has none to
    // measure against.
    std::optional<double> damage;
    std::optional<double> ehp;
    bool minionDamage = false;

    friend bool operator==(const PlaceChange &, const PlaceChange &) = default;
};

// By gear score or weights, for a ring, charm or jewel that goes in several
// places with something worn in at least two of them. bestSlot is the one the
// evaluation of the item chose. Empty for anything else.
QList<PlaceChange> gearPlaces(const Item &item, const Character &character, const PobPriorities &priorities,
                              const QString &bestSlot);

// By Path of Building, from the places it tried the item in. Empty unless it
// has several with something in at least two of them.
QList<PlaceChange> pobPlaces(const PobPlacements &tried);

} // namespace pou
