// SPDX-License-Identifier: MIT
#include "core/wearing.h"

#include "core/slots.h"

namespace pou {

QStringList slotsFor(const Item &item)
{
    if (item.slot.isEmpty())
        return {};
    return slotGroup(item.slot);
}

std::optional<Character> wear(const Character &character, const QString &slot, const Item &item)
{
    if (!slotsFor(item).contains(slot))
        return std::nullopt;

    Character c = character;
    if (const auto old = c.gear.constFind(slot); old != c.gear.cend() && c.resists)
        c.resists = rebaseResists(*c.resists, *old, item);

    // the game won't have both on
    const QString hand = otherHand(slot);
    const auto other = c.gear.constFind(hand);
    if (other != c.gear.cend() && (item.twoHanded || other->twoHanded))
        c = takeOff(c, hand);

    c.gear.insert(slot, item);
    c.fromPob.remove(slot);
    return c;
}

Character takeOff(const Character &character, const QString &slot)
{
    Character c = character;
    c.gear.remove(slot);
    c.fromPob.remove(slot);
    return c;
}

} // namespace pou
