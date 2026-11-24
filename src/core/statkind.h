// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace pou {

// What a stat is for, in the broad terms a glance can use.
enum class StatKind { Other, Resistance, Damage, Defence };

// The kind of a stat from its text, "Fire Resistance 30% or more" or
// "#% increased Attack Speed". Resistances come first, so "Penetrates 15% Fire
// Resistance" is damage, as it's what it's for.
StatKind statKindOf(const QString &text);

} // namespace pou
