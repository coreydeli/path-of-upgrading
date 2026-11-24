// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterdata.h"
#include "core/item.h"

#include <QJsonObject>
#include <QString>

namespace pou {

// What a character wears, from the character the game's account API gives
// (GET /character/poe2/<name>, the object inside "character").

// Our slot for one of its items, from where the item sits: "Helm" is
// "Helmet", the flask slot is the Life or Mana Flask by its place in it, and
// the jewels in the passive tree are Jewel 1 to 6 by their sockets. Empty
// for an item that isn't worn anywhere we have a slot for.
QString accountSlotOf(const QJsonObject &item);

// The item as the game's text, so it's scored like one you copied. Empty
// (raw is empty) for an item without a name or type.
Item accountItem(const QJsonObject &item, const QString &slot);

// Everything it wears in our slots: "equipment" and the jewels. Where two
// items claim one slot the first stays.
Equipped equipmentOf(const QJsonObject &character);

// The character with what it wears taken from the account, whatever it wore
// before, items from a build included. Weights, ignored stats, tweaks, the
// profile you picked and typed in resistances stay as they were. The name
// follows the one in the game unless you renamed it here.
Character withAccountGear(const Character &character, const AccountLink &link, const Equipped &gear);

} // namespace pou
