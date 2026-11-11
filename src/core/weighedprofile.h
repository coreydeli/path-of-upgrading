// SPDX-License-Identifier: MIT
#pragma once

#include "core/buildprofiles.h"
#include "core/slotmods.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// What Path of Building says one mod is worth to the build in a slot.
struct WeighedMod
{
    QString key;       // the stat key, "# to maximum Life"
    double weight = 0; // the build's gain per point of it
    double top = 0;    // the most the slot rolls of it, 0 when the answer doesn't say
};

// Everything Path of Building weighed for one slot group.
struct SlotWeighing
{
    QString group; // "Boots"
    QList<WeighedMod> mods;
};

inline constexpr QStringView weighedProfileId = u"weighed";

// A gear score profile made from Path of Building's weighings of the
// character's build, kept with the character. It rates the slot groups in
// `slots`; the others go by the profile it started from.
struct WeighedProfile
{
    BuildProfile profile; // the stats of the weighed groups only
    QStringList slots;
    QString pobVersion;      // of the Path of Building that weighed it
    QString date;            // ISO, when
    double damageWeight = 0; // the balance it was weighed by

    bool covers(const QString &group) const { return slots.contains(group); }
};

// A resistance the slot can roll counts at least this much, whatever the
// weighing says, as Path of Building's numbers don't see that you need it.
inline constexpr double resistFloor = 3;

// Turns the weighings into a profile. Within a group the stat whose weight
// times its excellent roll is highest is worth 10 and the rest in proportion,
// leaving out those under 0.5. An excellent roll is the best the slot's item
// class rolls in `mods`, or the answer's own `top`. A mod with neither is left
// out. A group with nothing left isn't in `slots`. Labels are the start
// profile's where it has the stat.
WeighedProfile weighedProfile(const QList<SlotWeighing> &weighings, const BuildProfile &start,
                              const SlotMods &mods = SlotMods::builtIn());

// The start profile's stats that aren't mods ("[Base] Armour"), which
// weighings don't cover, so they rate the weighed groups as before.
QList<ProfileStat> itemPropertyStats(const BuildProfile &start);

QJsonObject toJson(const WeighedProfile &weighed);
// Empty when the object isn't one.
std::optional<WeighedProfile> weighedProfileFromJson(const QJsonObject &o);

} // namespace pou
