// SPDX-License-Identifier: MIT
#pragma once

#include "core/ignore.h"
#include "core/item.h"
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

} // namespace pou
