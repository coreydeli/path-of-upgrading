// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"
#include "core/evaluation.h"
#include "core/item.h"
#include "core/pobverdict.h"

#include <QObject>

namespace pou {

// An item you checked and what it means for the character you play.
struct ItemCheck
{
    Item item;
    Evaluation evaluation;
};

// Judges the items you check for the character you play, by its gear scores
// or your weights.
class ItemJudge : public QObject
{
    Q_OBJECT

public:
    explicit ItemJudge(QObject *parent = nullptr);

    void setCharacter(const Character &character);
    // What matters to you, like resistances coming first.
    void setPriorities(const PobPriorities &priorities);

    // Judges the item in place of the one before.
    void check(const Item &item);
    const ItemCheck &current() const { return m_check; }

Q_SIGNALS:
    void checked(const ItemCheck &check);

private:
    Character m_character;
    PobPriorities m_priorities;
    ItemCheck m_check;
};

} // namespace pou
