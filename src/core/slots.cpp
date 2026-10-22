// SPDX-License-Identifier: MIT
#include "core/slots.h"

#include <QHash>
#include <algorithm>
#include <array>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

struct Group
{
    QString name;
    QStringList members;
};

const QList<Group> &groups()
{
    static const QList<Group> list = {
        {u"Weapon"_s, {u"Weapon"_s, u"Weapon 2"_s}},
        {u"Offhand"_s, {u"Offhand"_s, u"Offhand 2"_s}},
        {u"Ring"_s, {u"Ring 1"_s, u"Ring 2"_s}},
        {u"Charm"_s, {u"Charm 1"_s, u"Charm 2"_s, u"Charm 3"_s}},
        {u"Jewel"_s, {u"Jewel 1"_s, u"Jewel 2"_s, u"Jewel 3"_s, u"Jewel 4"_s, u"Jewel 5"_s, u"Jewel 6"_s}},
    };
    return list;
}

} // namespace

const QStringList &mainSlots()
{
    static const QStringList list = {
        u"Weapon"_s, u"Offhand"_s, u"Helmet"_s, u"Body Armour"_s, u"Gloves"_s,
        u"Boots"_s,  u"Belt"_s,    u"Amulet"_s, u"Ring 1"_s,      u"Ring 2"_s,
    };
    return list;
}

const QStringList &extraSlots()
{
    static const QStringList list = {
        u"Weapon 2"_s, u"Offhand 2"_s, u"Life Flask"_s, u"Mana Flask"_s, u"Charm 1"_s,
        u"Charm 2"_s,  u"Charm 3"_s,   u"Jewel 1"_s,    u"Jewel 2"_s,    u"Jewel 3"_s,
        u"Jewel 4"_s,  u"Jewel 5"_s,   u"Jewel 6"_s,
    };
    return list;
}

const QStringList &allSlots()
{
    static const QStringList list = mainSlots() + extraSlots();
    return list;
}

QStringList slotGroup(const QString &group)
{
    for (const Group &g : groups()) {
        if (g.name == group)
            return g.members;
    }
    return {group};
}

bool comparesWithWeakest(const QString &group)
{
    return group == u"Ring" || group == u"Charm" || group == u"Jewel";
}

QString groupOf(const QString &slot)
{
    for (const Group &g : groups()) {
        if (g.members.contains(slot))
            return g.name;
    }
    return slot;
}

QString slotForClass(const QString &itemClass)
{
    static const QHash<QString, QString> byClass = {
        {u"helmets"_s, u"Helmet"_s},
        {u"body armours"_s, u"Body Armour"_s},
        {u"gloves"_s, u"Gloves"_s},
        {u"boots"_s, u"Boots"_s},
        {u"belts"_s, u"Belt"_s},
        {u"amulets"_s, u"Amulet"_s},
        {u"rings"_s, u"Ring"_s},
        {u"shields"_s, u"Offhand"_s},
        {u"bucklers"_s, u"Offhand"_s},
        {u"foci"_s, u"Offhand"_s},
        {u"quivers"_s, u"Offhand"_s},
        {u"jewels"_s, u"Jewel"_s},
        {u"charms"_s, u"Charm"_s},
        {u"life flasks"_s, u"Life Flask"_s},
        {u"mana flasks"_s, u"Mana Flask"_s},
    };
    static const std::array weaponWords = {
        u"mace"_s,   u"sword"_s, u"axe"_s,      u"bow"_s,      u"crossbow"_s, u"quarterstave"_s,
        u"staves"_s, u"stave"_s, u"wand"_s,     u"sceptre"_s,  u"spear"_s,    u"flail"_s,
        u"dagger"_s, u"claw"_s,  u"talisman"_s, u"warstave"_s,
    };

    const QString c = itemClass.trimmed().toLower();
    if (c.isEmpty())
        return {};
    if (const auto it = byClass.constFind(c); it != byClass.cend())
        return *it;
    if (c.contains(u"jewel"))
        return u"Jewel"_s;
    if (c.contains(u"flask"))
        return c.contains(u"mana") ? u"Mana Flask"_s : u"Life Flask"_s;
    if (std::any_of(weaponWords.begin(), weaponWords.end(), [&](const QString &w) { return c.contains(w); }))
        return u"Weapon"_s;
    return {};
}

bool isTwoHanded(const QString &itemClass)
{
    static const std::array prefixes = {
        u"two hand"_s, u"quarterstaves"_s, u"staves"_s, u"crossbows"_s, u"warstaves"_s, u"talismans"_s,
    };
    const QString c = itemClass.trimmed().toLower();
    return std::any_of(prefixes.begin(), prefixes.end(), [&](const QString &p) { return c.startsWith(p); });
}

} // namespace pou
