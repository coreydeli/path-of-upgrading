// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"

#include <QString>

namespace pou {

// What a character's passive tree lets it do with its hands, as the build
// says. Items can give the first too, see twoHandedWithOffhand().
struct Wielding
{
    // Giant's Blood: two-handed axes, maces and swords in one hand.
    bool twoHandedInOne = false;
    // Instruments of Power, the Disciple of Varashta's: a focus with a staff.
    bool focusWithStaff = false;

    friend bool operator==(const Wielding &, const Wielding &) = default;
};

// Whether the weapon can be used with something in the offhand, or with it
// empty counting as it should. Two-handed weapons can't, unless the character
// can wield that kind in one hand: Giant's Blood from the passive tree or from
// an item worn (gear), or, for a staff, Instruments of Power and a focus.
// With no offhand item given, any that would do counts.
bool twoHandedWithOffhand(const Item &weapon, const Item *offhand, const Equipped &gear,
                          const Wielding &wielding);

// Whether the offhand slot is out of use, because the weapon in the weapon
// slot beside it is two-handed and nothing lets it have both.
bool offhandOutOfUse(const Equipped &gear, const Wielding &wielding, const QString &offhandSlot);

} // namespace pou
