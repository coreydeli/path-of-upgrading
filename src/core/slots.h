// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

namespace pou {

// Main gear first (these make up the gear score), then the extras.
const QStringList &mainSlots();
const QStringList &extraSlots();
const QStringList &allSlots();

// The slots an item of one kind can go in, e.g. "Ring" -> Ring 1, Ring 2.
// A slot that isn't part of a group is its own group.
QStringList slotGroup(const QString &group);

// Rings, charms and jewels are compared with the weakest one you wear rather
// than with the first slot of their group.
bool comparesWithWeakest(const QString &group);

// "Ring 2" -> "Ring", "Weapon 2" -> "Weapon", "Helmet" -> "Helmet".
QString groupOf(const QString &slot);

// The other hand of the same weapon set, "Weapon 2" -> "Offhand 2" and
// "Offhand" -> "Weapon". Empty for a slot that isn't a hand's.
QString otherHand(const QString &slot);

// Maps the "Item Class:" line of copied item text to a slot group. Empty for
// things that aren't gear (currency, gems, waystones...).
QString slotForClass(const QString &itemClass);
bool isTwoHanded(const QString &itemClass);

} // namespace pou
