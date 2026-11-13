// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// Main gear first (these make up the gear score), then the extras.
const QStringList &mainSlots();
const QStringList &extraSlots();
const QStringList &allSlots();

// The slots an item of one kind can go in, e.g. "Ring" -> Ring 1, Ring 2.
// A slot that isn't part of a group is its own group.
QStringList slotGroup(const QString &group);

// Where a main slot sits on the equipment screen the gear page draws, in a
// grid three wide. Read left to right, top to bottom, it's the order the slots
// are reached with the keyboard. Empty for a slot that isn't a main one.
struct EquipmentCell
{
    int row = 0;
    int column = 0;

    friend bool operator==(const EquipmentCell &, const EquipmentCell &) = default;
};

std::optional<EquipmentCell> equipmentCell(const QString &slot);

// Rings, charms and jewels are compared with the weakest one you wear rather
// than with the first slot of their group.
bool comparesWithWeakest(const QString &group);

// "Ring 2" -> "Ring", "Weapon 2" -> "Weapon", "Helmet" -> "Helmet".
QString groupOf(const QString &slot);

// How a slot is written for the user. The names above are what's saved and
// matched, so "Offhand" stays as it is there and reads "Off Hand" here.
QString slotLabel(const QString &slot);

// The other hand of the same weapon set, "Weapon 2" -> "Offhand 2" and
// "Offhand" -> "Weapon". Empty for a slot that isn't a hand's.
QString otherHand(const QString &slot);

// Maps the "Item Class:" line of copied item text to a slot group. Empty for
// things that aren't gear (currency, gems, waystones...).
QString slotForClass(const QString &itemClass);
bool isTwoHanded(const QString &itemClass);

} // namespace pou
