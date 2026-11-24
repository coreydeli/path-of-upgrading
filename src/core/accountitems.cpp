// SPDX-License-Identifier: MIT
#include "core/accountitems.h"

#include "core/listing.h"
#include "core/parser.h"
#include "core/pobbuild.h"

#include <QHash>
#include <QJsonArray>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

constexpr int jewelSlots = 6;

} // namespace

QString accountSlotOf(const QJsonObject &item)
{
    static const QHash<QString, QString> places = {
        {u"Weapon"_s, u"Weapon"_s},      {u"Offhand"_s, u"Offhand"_s}, {u"Weapon2"_s, u"Weapon 2"_s},
        {u"Offhand2"_s, u"Offhand 2"_s}, {u"Helm"_s, u"Helmet"_s},     {u"BodyArmour"_s, u"Body Armour"_s},
        {u"Gloves"_s, u"Gloves"_s},      {u"Boots"_s, u"Boots"_s},     {u"Belt"_s, u"Belt"_s},
        {u"Amulet"_s, u"Amulet"_s},      {u"Ring"_s, u"Ring 1"_s},     {u"Ring2"_s, u"Ring 2"_s},
    };
    const QString place = item.value(u"inventoryId").toString();
    const int x = item.value(u"x").toInt(-1);
    if (const auto it = places.constFind(place); it != places.cend())
        return *it;
    if (place == u"Flask") {
        // the life flask first and the mana flask beside it
        return x == 0 ? u"Life Flask"_s : x == 1 ? u"Mana Flask"_s : QString();
    }
    if (place == u"Charm" && x >= 0 && x < 3)
        return u"Charm %1"_s.arg(x + 1);
    if (place == u"PassiveJewels" && x >= 0 && x < jewelSlots)
        return u"Jewel %1"_s.arg(x + 1);
    return {};
}

Item accountItem(const QJsonObject &item, const QString &slot)
{
    const QString base = item.value(u"baseType").toString().isEmpty() ? item.value(u"typeLine").toString()
                                                                      : item.value(u"baseType").toString();
    return itemFromTradeJson(item, pobItemClass(slot, cleanListingText(base)));
}

Equipped equipmentOf(const QJsonObject &character)
{
    Equipped gear;
    const auto take = [&gear](const QJsonValue &value) {
        const QJsonObject o = value.toObject();
        const QString slot = accountSlotOf(o);
        if (slot.isEmpty() || gear.contains(slot))
            return;
        Item item = accountItem(o, slot);
        if (!item.raw.isEmpty())
            gear.insert(slot, std::move(item));
    };
    for (const QJsonValue &item : character.value(u"equipment").toArray())
        take(item);
    for (const QJsonValue &item : character.value(u"jewels").toArray())
        take(item);
    return gear;
}

Character withAccountGear(const Character &character, const AccountLink &link, const Equipped &gear)
{
    Character out = character;
    // the name you gave it stays, one that was the game's follows it
    if (out.name.isEmpty() || (out.linked && out.name == out.linked->name))
        out.name = link.name;
    out.linked = link;
    out.gear = gear;
    out.fromPob.clear();
    return out;
}

} // namespace pou
