// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "core/rates.h"

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

namespace pou {

// Listings from the trade site's /api/trade2/fetch, read into the same items
// as copied ones, so each can be scored against what you wear and handed to
// Path of Building.

// The site marks up its text for tooltips: "+39% to [Resistances|Lightning
// Resistance]" is "+39% to Lightning Resistance".
QString cleanListingText(const QString &text);

// The listed item, written out the way the game copies items and read with
// parseItem(). Listings don't say the item class, so it's the one searched for.
Item listingToItem(const QJsonObject &result, const QString &itemClass);

struct ListingPrice
{
    double amount = 0;
    QString currency; // as the site names it: "divine", "exalted", "chaos"
};

// What a price comes to in Exalted Orbs at rates from
// currencyRatesInExalts(). Empty for a currency without a rate.
std::optional<double> priceInExalts(const ListingPrice &price, const CurrencyRates &rates);

// "3 div", "50 ex", "2 chaos", or "no price".
QString formatPrice(const std::optional<ListingPrice> &price);

// What a price comes to in the other main currency, for listings in another
// one: "about 40 ex", or "about 1.4 div" from a divine's worth on. Empty when
// that says no more than the price does. Needs the price in exalts, see
// priceInExalts(), and what a divine is worth in them, or 0 when not known.
QString formatConvertedPrice(const ListingPrice &price, double exalts, double divineRate);

// One line of a listing's mods, as the site words it.
struct ListingMod
{
    QString text;
    // enchant, rune, implicit, fractured, explicit, desecrated or crafted
    QString tag;

    friend bool operator==(const ListingMod &, const ListingMod &) = default;
};

// One of the numbers an item has of its own, like its armour or damage per second.
struct ListingProperty
{
    QString name;  // "Armour"
    QString value; // "160"

    friend bool operator==(const ListingProperty &, const ListingProperty &) = default;
};

// The defences and weapon damage the item has of its own, as the game shows
// them on the item, with the weapon's damage per second after them. Those the
// item hasn't got, or has none of, are left out. A weapon's damage is split
// into physical and elemental only when it has both.
QList<ListingProperty> baseProperties(const Item &item);

struct Listing
{
    QString id;
    Item item;
    // Every mod line, in the order the game shows them. The item leaves out
    // some of those that aren't stats.
    QList<ListingMod> mods;
    QList<ListingProperty> properties; // see baseProperties()
    std::optional<ListingPrice> price;
    std::optional<double> exalts; // the price in Exalted Orbs, when there's a rate for it
    QString seller;               // the character they last played, or else the account
    QString account;              // "name#1234"
    bool online = false;
    // Listed in a merchant tab, bought from the seller's hideout without
    // asking them first. Those have no whisper.
    bool instantBuyout = false;
    QString whisper;      // what to send the seller in game, empty without one
    QString hideoutToken; // lets the site take you to an instant buyout seller's hideout
    QDateTime indexed;    // when the site last saw it listed
};

// "just now", "5 minutes ago", "1 hour ago", "3 days ago". Empty when the time
// isn't known. A time ahead of now counts as just now.
QString listedAgo(const QDateTime &indexed, const QDateTime &now);

// One element of a fetch answer's "result".
Listing readListing(const QJsonObject &result, const QString &itemClass, const CurrencyRates &rates);

// All of a fetch answer's listings, in its order. Those sold or taken down
// since the search come back as null and are left out.
QList<Listing> listingsFromJson(const QJsonObject &response, const QString &itemClass,
                                const CurrencyRates &rates);

} // namespace pou
