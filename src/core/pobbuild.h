// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <optional>

namespace pou {

// The build XML behind a Path of Building code, the text its Import/Export
// tab shares: the XML deflated and base64 encoded. XML pasted as it is
// passes through. Empty when it isn't a code.
std::optional<QString> decodeBuildCode(const QString &code);

// One item of a build, written out the way the game copies items so that
// parseItem() reads it like any other.
struct PobItemText
{
    QString text;
    // It came with its base numbers: armour, evasion, energy shield or
    // spirit. Path of Building never writes weapon damage.
    bool hasBase = false;
};

// raw is the item's text in the build and slot one of ours: "Weapon",
// "Ring 1", "Jewel 2"... Mods Path of Building keeps as a range, like
// "+(30-36) to maximum Life", come out at the roll it has for them.
PobItemText convertPobItem(const QString &raw, const QString &slot);

// The game's item class for an item in one of our slots: "Rings" in a ring
// slot, for weapons and off-hands the class of its base type ("Omen Sceptre"
// is a Sceptre). "Weapons" or "Off-hands" when the base is one it doesn't know.
QString pobItemClass(const QString &slot, const QString &baseType);

} // namespace pou
