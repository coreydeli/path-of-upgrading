// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"
#include "core/gearscore.h"
#include "core/pobverdict.h"
#include "core/scorer.h"

#include <QString>
#include <optional>

namespace pou {

// Judging an item for one of your characters, the way the popup shows it.

// How a character's gear is rated in one of its slots: by its build profile,
// tweaks and ignored stats, with each resistance counting only up to what you
// still need of it there. When you put resistances first, those you're short
// of count double unless you've tuned them yourself, and with Chaos
// Inoculation chaos resistance counts for nothing.
GearScoreOptions gearScoreOptions(const Character &character, const QString &slot,
                                  const PobPriorities &priorities = {});

// What an evaluation's scores are.
enum class ScoreMode {
    StatWeights,
    GearScores,
    // The delta is how much better Path of Building says the build gets, in
    // percent.
    PathOfBuilding,
};

struct Evaluation
{
    // Against what you wear in the slot the item would go in, with the
    // scores, delta and verdict of the mode.
    Comparison comparison;
    ScoreMode mode = ScoreMode::StatWeights;
    // The item's gear score in that slot, and the one of what you wear there.
    // Empty without a build profile.
    std::optional<GearScore> gear;
    std::optional<GearScore> gearEquipped;
};

// By gear score when the item and what you wear both have one, else by your
// weights. A ring, charm or jewel is compared with your weakest one by gear
// score, unless slot says which. slot can also be another slot the item
// goes in, like the offhand for a one-handed weapon.
Evaluation evaluate(const Item &item, const Character &character, const PobPriorities &priorities = {},
                    const QString &slot = {});

} // namespace pou
