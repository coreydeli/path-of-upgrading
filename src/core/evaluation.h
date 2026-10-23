// SPDX-License-Identifier: MIT
#pragma once

#include "core/character.h"
#include "core/characterstore.h"
#include "core/gearscore.h"
#include "core/pobeval.h"
#include "core/pobverdict.h"
#include "core/scorer.h"

#include <QList>
#include <QString>
#include <QStringList>
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
    // percent. The item's scores are still those of its gear score or your
    // weights.
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
    // What equipping it changes on your character. Empty for an item you
    // wear.
    std::optional<CharacterChange> change;
    // In words, what the score doesn't see that can make an upgrade a risk:
    // losing Spirit, minion skill levels or resistance you need.
    QStringList risks;
    // Resistances it leaves under the cap or further under it. Any of them
    // makes it a ResistLoss, whatever else it does.
    QList<ResistChange> resistLosses;
    // For sorting by resistance, as in PobVerdict: 2 caps one you're short
    // of, 1 adds toward a cap, -1 loses some you need and 0 is the rest.
    int resistRank = 0;
    std::optional<PobJudgement> pob;
};

// By gear score when the item and what you wear both have one, else by your
// weights. A ring, charm or jewel is compared with your weakest one by gear
// score, unless slot says which. slot can also be another slot the item
// goes in, like the offhand for a one-handed weapon. An upgrade with risks
// is Risky, and losing resistance you need is a ResistLoss.
Evaluation evaluate(const Item &item, const Character &character, const PobPriorities &priorities = {},
                    const QString &slot = {});

// Path of Building's judgement of the item, in place of the evaluation's
// own: its verdict, its score as the delta and what it breaks as the risks.
// A jewel goes in one of the build's sockets rather than your jewel slots.
// An item you wear stays Equipped.
Evaluation withPob(Evaluation evaluation, const PobJudgement &judgement);

} // namespace pou
