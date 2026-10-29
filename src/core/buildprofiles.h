// SPDX-License-Identifier: MIT
#pragma once

#include "core/resists.h"

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <initializer_list>
#include <optional>
#include <utility>

namespace pou {

// A value that depends on the slot group, like "10 on boots, 0 elsewhere".
class PerSlot
{
public:
    PerSlot(double everywhere = 0)
        : m_elsewhere(everywhere)
    {}
    PerSlot(std::initializer_list<std::pair<QString, double>> overrides, double elsewhere = 0)
        : m_overrides(overrides)
        , m_elsewhere(elsewhere)
    {}

    double at(const QString &slotGroup) const;

    // Slot groups where the value differs from elsewhere().
    const QList<std::pair<QString, double>> &overrides() const { return m_overrides; }
    double elsewhere() const { return m_elsewhere; }

private:
    QList<std::pair<QString, double>> m_overrides;
    double m_elsewhere = 0;
};

// One stat a build profile wants, with how much it matters in each slot
// (0 to 10, where 0 means it doesn't count there, usually because it can't
// roll there) and what an excellent roll of it is.
struct ProfileStat
{
    QString label;
    QRegularExpression pattern; // the stat keys it covers
    PerSlot importance;
    PerSlot cap;
    // The key to search and ignore it by. Empty for stats that cover several
    // mods without one obvious key, and for [Base] values.
    QString key;
    std::optional<Element> resist;
};

// What a kind of character wants from its gear, for the gear score.
struct BuildProfile
{
    QString id;
    QString name;
    QString summary;
    QList<ProfileStat> stats;
};

inline constexpr QStringView defaultBuildProfile = u"life-res";
// For scoring by your stat weights alone, as no profile has it.
inline constexpr QStringView noBuildProfile = u"none";

const QList<BuildProfile> &buildProfiles();
// The profile with that id; the default one for an empty id. Null when no
// profile has the id.
const BuildProfile *buildProfile(QStringView id);

// The slot groups the gear score rates, in the order of the slots.
const QStringList &ratedGroups();

// A profile's stat as you tweak it: once for each label, as tweaks go by
// label, with the slot groups it counts in.
struct TweakableStat
{
    QString label;
    QStringList groups;
};

// In the order of the profile's stats.
QList<TweakableStat> tweakableStats(const BuildProfile &profile);

} // namespace pou
