// SPDX-License-Identifier: MIT
#pragma once

#include "core/gearscore.h"
#include "core/ignore.h"
#include "core/item.h"
#include "core/resists.h"
#include "core/statsources.h"
#include "core/tradedata.h"
#include "core/weights.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// Search bodies for the official trade site, built from an item and what
// you want from it. They're POSTed to the search endpoint as they are.

// Which listings to show. Available is what the site shows by default:
// instant buyout and in person. Online leaves out merchant listings, which
// are most of the market.
enum class ListingStatus { Available, Securable, Online, Any };

QString listingStatusId(ListingStatus status);
// Available for anything unknown.
ListingStatus listingStatusFromId(QStringView id);

// What the trade site's data endpoints said.
struct TradeData
{
    StatIndex stats;
    TradeCategories categories;
};

struct SearchOptions
{
    ListingStatus status = ListingStatus::Available;
    // Stats ruled out with "exclude" become a group listings must have none of.
    QList<IgnoreRule> ignoreRules;
    // Armour and weapons have local versions of their defence and damage mods.
    bool preferLocal = false;
};

struct TradeQuery
{
    QJsonObject body;
    QStringList unmatched; // stats the trade site has no filter for
    int excluded = 0;      // stats listings must not have
};

// One stat you want, with the values you want it at.
struct StatPick
{
    QString key;
    std::optional<double> min;
    std::optional<double> max;
    // A trade stat picked by hand. When empty, it's matched from the key.
    QString id;
    QString idType;
    // Where the stat is on your item, to search for the same kind of mod.
    std::optional<StatOrigin> origin;
};

struct CustomSearchOptions : SearchOptions
{
    QString itemClass;
    QString baseType;
    bool matchBase = false; // only this base type, rather than anything of its category
};

// Items with all of the picked stats, cheapest first.
TradeQuery customSearch(const QList<StatPick> &picks, const TradeData &data,
                        const CustomSearchOptions &options);

struct WeightedSearchOptions : SearchOptions
{
    QString itemClass;         // the item's own when empty
    double minGainPercent = 0; // how far above your item's sum, in percent
};

struct WeightedSearch : TradeQuery
{
    double current = 0;  // your item's weighted sum, without rune and enchant bonuses
    double groupMin = 0; // what a listing's sum has to reach
};

// Your stat weights as the trade site's weighted sum: items whose weighted
// stats come to more than your item's, best first. Weapon DPS becomes a
// minimum. Base defences are left out, as they depend on the base type.
WeightedSearch weightedSearch(const Item &item, const Weights &weights, const TradeData &data,
                              const WeightedSearchOptions &options = {});

struct SimilarSearchOptions : SearchOptions
{
    // How loose the search is, from 0 (your values or higher) up to
    // similarSearchLevels() - 1. A search that finds nothing is retried looser.
    int level = 0;
};

struct SimilarSearch : TradeQuery
{
    QStringList skipped; // rune and enchant bonuses, which you can add yourself
    QStringList ignored;
    int need = 0;  // how many of the stats a listing has to have
    int total = 0; // how many stats were searched for
    int level = 0;
    QString levelLabel; // "90% of your values"
    bool unique = false;
};

int similarSearchLevels();

// "Same stats or better": each mod on your item at your value, in a count
// group so a listing may be without some of them. Base defences become a
// minimum, without your rune bonus. Uniques are searched by name.
SimilarSearch similarSearch(const Item &item, const TradeData &data,
                            const SimilarSearchOptions &options = {});

struct CraftBaseSearch : TradeQuery
{
    QStringList keyStats;          // labels of the stats asked for
    bool emptyAffixFilter = false; // whether the site could be asked for an open affix
};

// Bases to craft on: rare, uncorrupted items of the same kind that already
// have the slot's key stats at a decent roll, with an open prefix or suffix.
CraftBaseSearch craftBaseSearch(const Item &item, const QList<KeyStat> &keyStats, const TradeData &data,
                                const SearchOptions &options = {});

struct GearSearchOptions : SearchOptions
{
    QString itemClass; // the item's own when empty
    // Of your item's sum a listing has to reach: 1 beats it, 0.9 is within 10%.
    double share = 1;
    double baseShare = 0.8; // of your base defences
    // A decent roll of a key stat, against an excellent one. 0 leaves out
    // asking for the slot's key stats.
    double keyStatShare = 0.4;

    // Smaller searches, for when the trade site says one is too complex.
    bool explicitOnly = false;
    int maxFilters = 40;
    bool countOnly = false; // only "has some of the key stats", for the results to score
};

struct GearSearch : TradeQuery
{
    bool countOnly = false;
    double current = 0;  // your item's sum
    double groupMin = 0; // what a listing's sum has to reach
    int filters = 0;     // weighted stats searched for
    int dropped = 0;     // and left out for the site's limit
    QStringList parts;   // what the search looks at, for its summary
    QStringList keyStats;
    int needKeyStats = 0; // how many of them a listing has to have
};

// "Better for my build": the trade site's weighted sum with the gear score's
// weights, so it finds items that would score higher in the slot even with a
// different mix of stats. Implicit and explicit versions of a stat both count,
// and listings need some of the slot's key stats at a decent roll, so one big
// roll of a minor stat can't carry a poor item.
GearSearch gearSearch(const Item &item, const QList<SearchWeight> &weights, const TradeData &data,
                      const GearSearchOptions &options = {});

// A mod the trade site has, with what each point of it is worth.
struct ModWeight
{
    QString id; // "explicit.stat_3299347043"
    double weight = 0;
};

struct PobSearchOptions : SearchOptions
{
    QString itemClass;
    QString category; // for when there's none for the item class
    double min = 0;   // what a listing's sum has to reach
    // How many filters it has at most, as the trade site limits them. The
    // heaviest mods are kept.
    int maxFilters = 30;
    // Resistances you're short of. Their mods count for at least half as much
    // as the best mod of a percentage, so items with your damage and the
    // resistance come first, rather than those with only a big roll of it.
    QList<Element> shortResists;
    // The stats on what you wear in the slot. A listing needs as many of the
    // build's damage mods as it has, up to two, so it doesn't trade them for
    // resistance. Empty asks for none.
    QStringList worn;
};

struct PobSearch : TradeQuery
{
    double groupMin = 0;
    int filters = 0; // weighted mods searched for
    int dropped = 0; // and left out for the site's limit
    bool resistsBoosted = false;
    int needDamage = 0; // of the build's damage mods a listing has to have
};

// "Better for my build" as Path of Building's own trade searches find it:
// the trade site's weighted sum of the mods it weighed, best first. Mods the
// site doesn't have and those you ignore are left out.
PobSearch pobSearch(const QList<ModWeight> &mods, const TradeData &data,
                    const PobSearchOptions &options = {});

} // namespace pou
