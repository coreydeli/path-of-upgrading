// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobverdict.h"
#include "core/tradequery.h"
#include "core/weighedprofile.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace pou {

// Path of Building's own trade searches weigh each mod the trade site has by
// trying it on a blank item in the slot, see weigh in pobbridge.lua.

// One of the numbers the mods are weighed by, with how much it counts.
struct PobStatWeight
{
    QString stat; // Path of Building's name for it
    double weight = 0;

    friend bool operator==(const PobStatWeight &, const PobStatWeight &) = default;
};

// The build's damage, by the number items are judged by (see
// pobDamageStat()), counts for damageWeight and its effective HP for the
// rest, though never for nothing.
QList<PobStatWeight> pobSearchStats(const PobStats &build, double damageWeight);

// What the answer says of a mod besides its weight.
struct PobModAbout
{
    QString text;   // as the trade site words it, "+# to maximum Life"
    double top = 0; // the most the slot rolls of it, 0 when it doesn't say
};

struct PobWeighing
{
    QString category; // the trade site's for the slot, "armour.gloves"
    // The least a listing's sum should come to, from what you wear there.
    double min = 0;
    QList<ModWeight> mods;             // those whose usual roll adds the most first
    QHash<QString, PobModAbout> about; // by mod id, for those that say
};

// What the weighing says of the slot group, for making a gear score profile.
// Mods that don't say what they are, or that no stat key can be made of, are
// left out.
SlotWeighing slotWeighing(const QString &group, const PobWeighing &weighing);

// Mods without an id or a weight above nothing are left out.
PobWeighing readPobWeighing(const QJsonObject &answer);

} // namespace pou
