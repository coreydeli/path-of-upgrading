// SPDX-License-Identifier: MIT
// Made-up waystones and tablets, shaped like the game's copied text.
#pragma once

#include <QString>

using namespace Qt::Literals::StringLiterals;

namespace maptexts {

inline const QString header = u"Item Class: Waystones\nRarity: Rare\nGrim Haven\nWaystone (Tier 15)\n"
                              u"--------\nWaystone Tier: 15\nRevives Available: 4 (augmented)\n"
                              u"--------\nItem Level: 79\n--------\n"_s;

// no mod on it that hurts
inline const QString good = header +
                            u"Monsters have 30% increased Accuracy Rating\n7% more Pack size\n"
                            u"10% more Waystones found in Area\n"
                            u"Monsters are Evasive\n6% more Pack size\n15% more Waystones found in Area"_s;

// one risky mod among four
inline const QString fine = header +
                            u"Monsters have 30% increased Accuracy Rating\n7% more Pack size\n"
                            u"10% more Waystones found in Area\n"
                            u"Monsters are Evasive\n6% more Pack size\n15% more Waystones found in Area\n"
                            u"Players are periodically Cursed with Temporal Chains\n8% more Pack size\n"
                            u"20% more Waystones found in Area\n"
                            u"12% more Monster Life\n23% more Magic and Rare Monsters\n"
                            u"Rare Monsters have 23% more chance of Monster Modifiers\n"
                            u"15% more Waystones found in Area"_s;

// a mod that can end the run among four
inline const QString bad =
    header + u"Monsters have 30% increased Accuracy Rating\n7% more Pack size\n"
             u"10% more Waystones found in Area\n"
             u"Monsters are Evasive\n6% more Pack size\n15% more Waystones found in Area\n"
             u"Players have 36% less Recovery Rate of Life and Energy Shield\n"
             u"15% more Rarity of Items found in this Area\n20% more Waystones found in Area\n"
             u"Monsters deal 12% of Damage as Extra Fire\nMonsters have 16% more Effectiveness\n"
             u"15% more Waystones found in Area"_s;

// the same as an advanced copy, where each mod says prefix or suffix
inline const QString advanced = u"Item Class: Waystones\nRarity: Rare\nGrim Haven\nWaystone (Tier 15)\n"
                                u"--------\nWaystone Tier: 15\n--------\nItem Level: 79\n--------\n"
                                u"{ Prefix Modifier \"Heated\" (Tier: 1) }\n"
                                u"Monsters deal 12(10-14)% of Damage as Extra Fire\n"
                                u"Monsters have 16% more Effectiveness\n15% more Waystones found in Area\n"
                                u"{ Suffix Modifier \"of Ease\" (Tier: 1) }\n"
                                u"Monsters are Evasive\n6% more Pack size\n"
                                u"{ Suffix Modifier \"of Pain\" (Tier: 1) }\n"
                                u"Players have 32(30-35)% less Recovery Rate of Life and Energy Shield\n"
                                u"15% more Rarity of Items found in this Area\n"
                                u"{ Prefix Modifier \"Lucky\" (Tier: 1) }\n"
                                u"Monsters have 15(10-20)% increased Accuracy Rating\n7% more Pack size"_s;

// a desecrated waystone: two of the new suffixes and an abyssal prefix, as poe2db words them
inline const QString desecrated =
    header + u"Area contains 3(2-3) additional Abysses\nAbysses lead to an Abyssal Boss\n"
             u"Players and their Minions deal no damage for 3 out of every 10 seconds\n"
             u"Players are Marked for Death for 10 seconds\nafter killing a Rare or Unique monster\n"
             u"Natural Rare Monsters in Area Eat the Souls of slain Monsters in their Presence\n"
             u"Players steal the Eaten Souls of Slain Rare Monsters in Area"_s;

// a mod with two stats, and rolls that take both
inline const QString pairs = header +
                             u"Monsters have 15% increased Ailment Threshold\n"
                             u"Monsters have 14% increased Stun Threshold\n"
                             u"Monsters have 13% more Effectiveness\n10% more Waystones found in Area\n"
                             u"Monsters have 90% increased Critical Hit Chance\n"
                             u"Monsters have +12% Critical Damage Bonus\n9% more Pack size\n"
                             u"15% more Waystones found in Area"_s;

inline const QString tablet = u"Item Class: Tablet\nRarity: Magic\nBurning Breach Precursor Tablet\n"
                              u"--------\nUses Remaining: 10\nAdds an Otherworldy Breach to a Map\n"
                              u"--------\nItem Level: 79\n--------\n"
                              u"Map has 30% increased Magic Monsters\n"
                              u"Map has 1 additional random Modifiers"_s;

} // namespace maptexts
