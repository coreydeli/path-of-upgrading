// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "core/resists.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

namespace pou {

// "If you equip it": what happens to your character's totals when an item
// replaces what's in its slots.

struct TotalChange
{
    QString label; // "Spirit", "Life"...
    double from = 0;
    double to = 0;
    double delta = 0;
};

struct ResistChange
{
    Element element = Element::Fire;
    QString label; // "Fire res"
    double delta = 0;
    double cap = 75;
    // the rest needs your resistances to be known
    std::optional<double> from;
    std::optional<double> to;
    double safe = 75; // the cap plus the buffer
    bool under = false;
    bool wasUnder = false;
    bool thin = false; // capped, but by less than the buffer
    bool wasThin = false;
};

struct CharacterChange
{
    QStringList targets; // the slots it replaces
    QList<TotalChange> totals;
    QList<ResistChange> resists;
    std::optional<TotalChange> overall; // average gear score over the main slots
    QStringList warnings;
};

// A slot's gear score, when there is one.
using GearScoreFn = std::function<std::optional<double>(const Item &item, const QString &slot)>;

// `targets` are the slots the item replaces; it goes into the first.
std::optional<CharacterChange> changeIfEquipped(const Equipped &equipped, const Item &item,
                                                const QStringList &targets,
                                                const LiveResists *resists = nullptr,
                                                const GearScoreFn &gearScore = {});

} // namespace pou
