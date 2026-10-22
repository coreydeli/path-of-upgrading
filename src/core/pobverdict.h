// SPDX-License-Identifier: MIT
#pragma once

#include "core/verdict.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// Judging an item by what Path of Building calculates for your build when it
// takes the place of what you wear. More damage and more effective HP make it
// better.

// Numbers Path of Building calculated for a build, by its own names for them:
// "FullDPS", "TotalEHP", "FireResist", "MissingFireResist"... The minions'
// damage is "MinionDPS" and their life "MinionLife". Numbers it didn't
// calculate are missing.
using PobStats = QHash<QString, double>;

// What matters to you besides damage and effective HP.
struct PobPriorities
{
    bool keepDamage = true; // an item that costs a good part of your damage is never an upgrade
};

struct PobVerdictOptions
{
    // How much of the score is damage, from 0 to 1. Effective HP is the rest.
    double damageWeight = 0.6;
    PobPriorities priorities;
};

// One of the numbers, before and after the swap.
struct PobChange
{
    QString stat; // Path of Building's name for it
    double from = 0;
    double to = 0;
    // The change against from, rounded to a decimal. Unknown unless from is
    // above 0.
    std::optional<double> percent;
};

struct PobVerdict
{
    Verdict verdict = Verdict::Same;
    // How much better it makes the build in percent, rounded to a decimal.
    double score = 0;
    // What it breaks, in words.
    QStringList breaks;

    // The number the build's damage is judged by, see pobDamageStat().
    PobChange damage;
    bool losesDamage = false;
    // Unknown when Path of Building didn't calculate it.
    std::optional<PobChange> ehp;

    // Of Life, Energy Shield, Mana, Spirit, unreserved Spirit and minion Life
    // those that change by a point or more.
    QList<PobChange> changes;
};

// The number a build's damage is judged by: "FullDPS" when Path of Building
// adds up its skills, else "MinionDPS" when it has minions, else its main
// skill's "CombinedDPS" or "TotalDPS".
QString pobDamageStat(const PobStats &stats);

// before is the build as it is, after the build with the item.
PobVerdict judgeWithPob(const PobStats &before, const PobStats &after, const PobVerdictOptions &options = {});

} // namespace pou
