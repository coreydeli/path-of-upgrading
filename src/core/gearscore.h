// SPDX-License-Identifier: MIT
#pragma once

#include "core/buildprofiles.h"
#include "core/item.h"
#include "core/resists.h"
#include "core/scorer.h"
#include "core/slotmods.h"
#include "core/statsources.h"
#include "core/weighedprofile.h"

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// The gear score rates an item from 0 to 100 for how useful it is to your
// build in its slot. Each stat the build profile wants scores
//   importance x roll quality (the roll against an excellent one),
// and the total is measured against the best a slot can do: an item with
// the slot's most important stats at excellent rolls. Stats the profile
// doesn't rate add nothing, and are listed as not scored. That says nothing
// about whether they help.

// Importance multipliers by profile stat label, set in the gear score editor.
using ProfileTweaks = QHash<QString, double>;

struct GearScoreOptions
{
    QString build; // profile id, empty for the default profile
    // What Path of Building weighed for the build. It rates the slot groups it
    // covers, and the profile above the others.
    std::optional<WeighedProfile> weighed;
    ProfileTweaks tweaks;
    StatFilter ignore;
    // How much more of each resistance you need to be capped, where known.
    // A resistance only counts up to that, and not at all once you're capped.
    PerElement<std::optional<double>> resistNeed;
    // What items can roll, for the notes. The shipped table when null.
    const SlotMods *mods = nullptr;
};

struct GearContribution
{
    QString key;
    QString label; // the profile stat it counts as
    double value = 0;
    int quality = 0;   // percent of an excellent roll, up to 115
    double points = 0; // rounded to one decimal
    double best = 0;   // the most the item's class rolls of it, 0 when that's not known
    StatOrigin origin = StatOrigin::Mod;
};

struct GearScore
{
    enum class Unrated { No, FlaskOrCharm, Unidentified, Unique };

    QString build;
    QString slot; // slot group
    Unrated unrated = Unrated::No;
    int score = 0;
    QString grade;                         // S, A, B, C or D; empty when unrated
    QList<GearContribution> contributions; // most points first
    QStringList notScored;                 // mods the profile doesn't rate
    QStringList missing;                   // up to three important stats the item doesn't have
    QHash<QString, double> missingBest;    // for each of those, the most its class rolls of it
    double ideal = 0;                      // points of an excellent item for the slot
    QString resistNote;                    // why some resistance counted for less

    bool isRated() const { return unrated == Unrated::No; }
};

// Empty when the profile doesn't exist or the item isn't gear.
std::optional<GearScore> gearScore(const Item &item, const GearScoreOptions &options = {});

QString gearGrade(int score);

// The stats that keep a gear score down, one to a string. The notes below are
// built on them.
//   the mods the profile doesn't rate, by name: "#% increased Rarity of Items found"
QStringList notScoredStats(const GearScore &gear);
//   the missing ones, with the most the item can roll: "Life (up to 149)"
QStringList missingStats(const GearScore &gear);
//   the weakest rolls under that share of an excellent roll, "Life 62 of up to
//   149", or "Life 40% of an excellent roll" when it isn't known what the
//   item can roll
QStringList weakRolls(const GearScore &gear, int underPercent = 50);

// What would raise a gear score, in the words the popup uses. Each is empty
// when there's nothing to say.
//   "Missing what your build wants: Life (up to 149), Fire Resistance (up to 45)."
QString missingNote(const GearScore &gear);
//   "Not in the gear score: #% increased Rarity of Items found."
QString notScoredNote(const GearScore &gear);
// The stats rolled under a share of an excellent roll, the weakest first and
// three at most, "Weak rolls: Life 62 of up to 149, Cold Resistance 35% of an
// excellent roll." Runes aren't rolls, so they're left out.
QString weakRollsNote(const GearScore &gear, int underPercent = 50);

// Points per unit of a stat for the build in that slot, with your tweaks; 0
// when the build doesn't want it there.
double usefulness(const QString &slot, const QString &key, const GearScoreOptions &options = {});

struct KeyStat
{
    QString key;
    QString label;
    double importance = 0;
    double cap = 0;
};

// The build's most important stats for a slot that can be searched for by
// key, most important first. Resistances are left out.
QList<KeyStat> topStats(const QString &slot, int count = 2, const GearScoreOptions &options = {});

// The least of a stat worth searching for, from how much a top roll of it
// gives: a bit over half of it, and at least 1.
double searchMinimum(double cap);

// One part of a slot's gear score, as a trade search weight.
struct SearchWeight
{
    QString label;
    QRegularExpression pattern;
    QString key;
    double weight = 0; // points per unit
    double importance = 0;
    double cap = 1;
    std::optional<Element> resist;
    std::optional<double> need; // for resistances: how much more you need, where known
};

// What a slot's gear score is made of. Ignored stats and resistances you're
// already capped on are left out.
QList<SearchWeight> searchWeights(const QString &slot, const GearScoreOptions &options = {});

} // namespace pou
