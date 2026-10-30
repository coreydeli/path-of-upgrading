// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"
#include "core/pobbuild.h"

namespace pou {

// What importing a Path of Building build did to one of your characters.
struct BuildImport
{
    Character character;
    int items = 0; // put on from the build
    // Items you copied in the game that the build has too. They stay, as the
    // game's copy says more, like a weapon's damage.
    int kept = 0;
    // Weapons put on from the build, which can't say what damage they deal.
    int withoutDamage = 0;
    bool resists = false; // the build's resistances were taken
};

// The character wearing the build's gear in place of what came with its
// build before, the jewels in the order of their sockets. The build's
// resistances are taken unless yours were read off the character sheet.
BuildImport importBuild(const Character &character, const PobBuild &build);

// The gear score profile that suits the build: the minion one when the
// minions of its main skill deal damage, the Energy Shield one when it has
// more than twice as much Energy Shield as Life, and the default one
// otherwise.
QString fittingProfile(const PobBuild &build);

// The build's resistances with the gear it wears, as importing it takes them.
// The maximum and margin of the previous ones stay. Empty when the build has
// none.
std::optional<ResistRecord> buildResists(const PobBuild &build, const ResistRecord *previous = nullptr);

} // namespace pou
