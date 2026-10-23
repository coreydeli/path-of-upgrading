// SPDX-License-Identifier: MIT
// Item texts copied from the game, shared by several tests.
#pragma once

#include <QString>
#include <QStringList>

using namespace Qt::Literals::StringLiterals;

namespace itemtexts {

inline const QString helmetNew = uR"item(Item Class: Helmets
Rarity: Rare
Doom Visage
Expert Visored Helm
--------
Quality: +20% (augmented)
Armour: 312 (augmented)
--------
Requirements:
Level: 65
Str: 101
--------
Sockets: S
--------
Item Level: 79
--------
+12% to Cold Resistance (rune)
--------
+89 to maximum Life
+32% to Fire Resistance
+11% to all Elemental Resistances
+14 to Strength
12% reduced Attribute Requirements)item"_s;

// written with Windows line endings, as some tools copy them
inline const QString helmetOld = uR"item(Item Class: Helmets
Rarity: Rare
Grim Crown
Visored Helm
--------
Armour: 180
--------
Item Level: 50
--------
+55 to maximum Life
+20% to Fire Resistance
+18% to Lightning Resistance)item"_s.split(u'\n')
                                     .join(u"\r\n"_s);

inline const QString advancedRing = uR"item(Item Class: Rings
Rarity: Rare
Storm Loop
Sapphire Ring
--------
Requirements:
Level: 40
--------
Item Level: 75
--------
{ Implicit Modifier }
+24(20-30)% to Cold Resistance (implicit)
--------
{ Prefix Modifier "Hale" (Tier: 4) — Life }
+62(55-69) to maximum Life
{ Suffix Modifier "of the Squall" (Tier: 3) — Elemental, Lightning, Resistance }
+30(26-30)% to Lightning Resistance
{ Suffix Modifier "of the Fox" — Attribute }
+12(9-15) to Strength and Dexterity)item"_s;

inline const QString twoHandMace = uR"item(Item Class: Two Hand Maces
Rarity: Rare
Brute Knell
Forge Maul
--------
Physical Damage: 100-160 (augmented)
Fire Damage: 20-40 (augmented)
Critical Hit Chance: 5.00%
Attacks per Second: 1.10
--------
Item Level: 70
--------
120% increased Physical Damage
Adds 20 to 40 Fire Damage
+40 to Strength)item"_s;

} // namespace itemtexts
