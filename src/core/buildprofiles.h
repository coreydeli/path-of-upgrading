// SPDX-License-Identifier: MIT
#pragma once

#include "core/resists.h"

#include <QList>
#include <QRegularExpression>
#include <QString>
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

const QList<BuildProfile> &buildProfiles();
// The profile with that id; the default one for an empty id. Null when no
// profile has the id.
const BuildProfile *buildProfile(QStringView id);

} // namespace pou
