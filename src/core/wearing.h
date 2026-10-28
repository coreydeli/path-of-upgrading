// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"

#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// The slots the item can be worn in: either ring slot for a ring, either
// weapon set for a weapon. None for an item that isn't gear.
QStringList slotsFor(const Item &item);

// The character wearing the item in the slot, in place of what was there,
// the build's or one you copied. A two-handed weapon takes the offhand off
// with it, and an offhand a two-handed weapon. The same item copied again
// leaves your resistances as they were, see rebaseResists(). Empty when the
// item can't go in the slot.
std::optional<Character> wear(const Character &character, const QString &slot, const Item &item);
// The character with nothing in the slot.
Character takeOff(const Character &character, const QString &slot);

} // namespace pou
