// SPDX-License-Identifier: MIT
#pragma once

#include "core/resists.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QUrl>
#include <optional>

namespace pou {

// The build XML behind a Path of Building code, the text its Import/Export
// tab shares: the XML deflated and base64 encoded. XML pasted as it is
// passes through. Empty when it isn't a code.
std::optional<QString> decodeBuildCode(const QString &code);

// Where the code behind a link to a shared build downloads from, for the
// sites Path of Building shares builds on: pobb.in, poe.ninja, Maxroll and
// the like. Empty when the link isn't to one of them.
QUrl buildCodeUrl(const QString &link);

// One item of a build, written out the way the game copies items so that
// parseItem() reads it like any other.
struct PobItemText
{
    QString text;
    // It's a weapon that attacks. Path of Building doesn't save weapon
    // damage, so the text can't say what it deals; copying it in game does.
    bool missingDamage = false;
};

// raw is the item's text in the build and slot one of ours: "Weapon",
// "Ring 1", "Jewel 2"... Mods Path of Building keeps as a range, like
// "+(30-36) to maximum Life", come out at the roll it has for them.
PobItemText convertPobItem(const QString &raw, const QString &slot);

// The game's item class for an item in one of our slots: "Rings" in a ring
// slot, for weapons and off-hands the class of its base type ("Omen Sceptre"
// is a Sceptre). "Weapons" or "Off-hands" when the base is one it doesn't know.
QString pobItemClass(const QString &slot, const QString &baseType);

// What a build wears: the item set it has active and the jewels in its
// passive tree.
struct PobBuild
{
    int level = 0;
    QString className;  // "Sorceress"
    QString ascendancy; // "Stormweaver", empty before it has one

    // By slot: "Weapon", "Ring 1", "Life Flask", "Charm 2"... The weapon set
    // in use is "Weapon" and "Offhand", the other one "Weapon 2" and "Offhand 2".
    QHash<QString, PobItemText> gear;
    // The set in use is the one Path of Building calls its second, with the
    // slots "Weapon 1 Swap" and "Weapon 2 Swap".
    bool swapInUse = false;
    // In the order of the nodes they're socketed in.
    QList<PobItemText> jewels;

    // As Path of Building shows them plus however far over the maximum they
    // go. Unknown when the build doesn't say.
    PerElement<std::optional<double>> resists;
};

// Empty when the XML isn't a Path of Building 2 build.
std::optional<PobBuild> parsePobBuild(const QString &xml);

// Our slot for one of Path of Building's, empty for those we don't keep,
// like "Ring 3". Which of its weapon sets is our "Weapon" depends on the
// build's swapInUse.
QString gearSlot(const QString &pobSlot, bool swapInUse);
// Path of Building's slot for one of ours. Empty for the jewels, since it
// names its sockets after the passives they're on.
QString pobSlot(const QString &slot, bool swapInUse);

// The XML is a build for the first game, which the first Path of Building
// shares in codes just like these.
bool isPoe1Build(const QString &xml);

} // namespace pou
