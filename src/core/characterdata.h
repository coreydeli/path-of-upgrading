// SPDX-License-Identifier: MIT
#pragma once

#include "core/gearscore.h"
#include "core/hands.h"
#include "core/ignore.h"
#include "core/item.h"
#include "core/resists.h"
#include "core/weights.h"

#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <optional>

namespace pou {

// One of your characters, with everything kept for it between runs: what it
// wears, how items are judged for it and its resistances.
struct Character
{
    QString name;
    Equipped gear;
    // Slots whose item came with the Path of Building build rather than from
    // the game.
    QSet<QString> fromPob;
    Weights weights = defaultWeights();
    QList<IgnoreRule> ignored;
    // The gear score profile the weighing starts from, and rates what it
    // hasn't weighed, by id. Empty for the default.
    QString buildProfile;
    // You chose the profile yourself, so importing a build leaves it be.
    bool profilePicked = false;
    // Items are judged by your stat weights alone, with no gear score.
    bool weightsOnly = false;
    ProfileTweaks tweaks;
    // What Path of Building weighed for the build, once it has.
    std::optional<WeighedProfile> weighed;
    std::optional<ResistRecord> resists;
    // Path of Building found Chaos Inoculation the last time it loaded the
    // build. Kept for when it isn't running.
    bool chaosInoculation = false;
    // What the build's passive tree lets it do with its hands, imported with it.
    Wielding wielding;
};

// Gear is kept as the game's text and read again with parseItem(), so items
// saved long ago gain from what the parser has learned since. An item it
// can't read any more is left out.
QJsonObject toJson(const Character &character);
// Empty for a character saved by a newer version of the app, in a way this
// one doesn't know.
std::optional<Character> characterFromJson(const QJsonObject &o);

} // namespace pou
