// SPDX-License-Identifier: MIT
#pragma once

#include "core/buildprofiles.h"
#include "core/item.h"
#include "core/resists.h"
#include "core/scorer.h"
#include "core/statsources.h"

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
// the slot's most important stats at excellent rolls. Stats the build has
// no use for add nothing and are listed as wasted.

// Importance multipliers by profile stat label, set in the gear score editor.
using ProfileTweaks = QHash<QString, double>;

struct GearScoreOptions
{
    QString build; // profile id, empty for the default profile
    ProfileTweaks tweaks;
    StatFilter ignore;
    // How much more of each resistance you need to be capped, where known.
    // A resistance only counts up to that, and not at all once you're capped.
    PerElement<std::optional<double>> resistNeed;
};

struct GearContribution
{
    QString key;
    QString label; // the profile stat it counts as
    double value = 0;
    int quality = 0;   // percent of an excellent roll, up to 115
    double points = 0; // rounded to one decimal
    StatOrigin origin = StatOrigin::Mod;
};

struct GearScore
{
    enum class Unrated { No, FlaskOrCharm, Unidentified, Unique };

    QString build;
    QString buildName;
    QString slot; // slot group
    Unrated unrated = Unrated::No;
    int score = 0;
    QString grade;                         // S, A, B, C or D; empty when unrated
    QList<GearContribution> contributions; // most points first
    QStringList wasted;                    // mods the build has no use for
    QStringList missing;                   // up to three important stats the item doesn't have
    double ideal = 0;                      // points of an excellent item for the slot
    QString resistNote;                    // why some resistance counted for less

    bool isRated() const { return unrated == Unrated::No; }
};

// Empty when the profile doesn't exist or the item isn't gear.
std::optional<GearScore> gearScore(const Item &item, const GearScoreOptions &options = {});

QString gearGrade(int score);

// Points per unit of a stat for the build in that slot; 0 when the build
// doesn't want it there.
double usefulness(const QString &slot, const QString &key, QStringView build = {});

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
