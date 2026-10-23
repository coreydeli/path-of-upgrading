// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"
#include "core/gearscore.h"
#include "core/pobverdict.h"

#include <QString>

namespace pou {

// Judging an item for one of your characters, the way the popup shows it.

// How a character's gear is rated in one of its slots: by its build profile,
// tweaks and ignored stats, with each resistance counting only up to what you
// still need of it there. When you put resistances first, those you're short
// of count double unless you've tuned them yourself, and with Chaos
// Inoculation chaos resistance counts for nothing.
GearScoreOptions gearScoreOptions(const Character &character, const QString &slot,
                                  const PobPriorities &priorities = {});

} // namespace pou
