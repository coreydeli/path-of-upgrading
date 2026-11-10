// SPDX-License-Identifier: MIT
#include "core/hands.h"

#include "core/slots.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

bool classStartsWith(const Item &item, QStringView prefix)
{
    return item.itemClass.trimmed().startsWith(prefix, Qt::CaseInsensitive);
}

bool givesGiantsBlood(const Item &item)
{
    return item.raw.contains(u"You can wield Two-Handed Axes, Maces and Swords in one hand"_s,
                             Qt::CaseInsensitive);
}

bool wieldedInOneByGiantsBlood(const Item &weapon)
{
    return classStartsWith(weapon, u"two hand axes") || classStartsWith(weapon, u"two hand maces") ||
           classStartsWith(weapon, u"two hand swords");
}

} // namespace

bool twoHandedWithOffhand(const Item &weapon, const Item *offhand, const Equipped &gear,
                          const Wielding &wielding)
{
    if (!weapon.twoHanded)
        return true;

    bool giantsBlood = wielding.twoHandedInOne;
    for (auto it = gear.cbegin(); !giantsBlood && it != gear.cend(); ++it)
        giantsBlood = givesGiantsBlood(it.value());
    if (giantsBlood && wieldedInOneByGiantsBlood(weapon))
        return true;

    return wielding.focusWithStaff && classStartsWith(weapon, u"staves") &&
           (!offhand || classStartsWith(*offhand, u"foci"));
}

bool offhandOutOfUse(const Equipped &gear, const Wielding &wielding, const QString &offhandSlot)
{
    const QString hand = otherHand(offhandSlot);
    if (hand.isEmpty() || groupOf(offhandSlot) != u"Offhand")
        return false;
    const auto weapon = gear.constFind(hand);
    return weapon != gear.cend() && !twoHandedWithOffhand(*weapon, nullptr, gear, wielding);
}

} // namespace pou
