// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "core/resists.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// Rating a waystone or a precursor tablet: what each of its mods gives, how
// dangerous it is to run, and what to do about the bad ones. It works from the
// item's text alone. The mods are the ones poe2db.tw lists for waystones and
// for the Precursor Tower mods tablets roll, and the danger of each is ours.

enum class MapKind { Waystone, Tablet };

// How much a mod can hurt you in the map.
enum class Danger {
    Harmless,
    Mild,   // a bit harder, nothing to worry about
    Risky,  // worth thinking about
    Deadly, // can end the run
};

enum class MapGrade { Good, Fine, Bad };

enum class AffixSide { Unknown, Prefix, Suffix };

// What the rating knows of your character. Nothing at all is fine.
struct MapBuild
{
    // a build is imported, so what it says counts
    bool imported = false;
    std::optional<LiveResists> resists;
    // Life plus Energy Shield, zero when unknown
    double health = 0;
};

struct MapMod
{
    QStringList lines; // as the game wrote them, without the roll ranges
    AffixSide side = AffixSide::Unknown;
    QStringList gives;                // "15% more Waystones found"
    QString costs;                    // what it does to you, plain, empty when nothing
    Danger danger = Danger::Harmless; // with your build counted
    Danger baseDanger = Danger::Harmless;
    QString buildNote; // why your build makes it worse, empty when it doesn't
    bool known = true; // false for a mod the table doesn't have

    bool isBad() const { return danger >= Danger::Risky; }
};

struct MapCheck
{
    MapKind kind = MapKind::Waystone;
    MapGrade grade = MapGrade::Fine;
    QString summary; // why it's graded so
    QList<MapMod> mods;
    QStringList suggestions;
    bool corrupted = false;
    // the build was there to count, and whether it changed anything
    bool usedBuild = false;
    bool buildChanged = false;

    int badMods() const;
};

// Empty when the item is neither.
std::optional<MapKind> mapKindOf(const Item &item);

MapCheck checkMap(const Item &item, MapKind kind, const MapBuild &build = {});

// "1 in 3"
QString oddsText(int bad, int total);

QString dangerName(Danger danger);

} // namespace pou
