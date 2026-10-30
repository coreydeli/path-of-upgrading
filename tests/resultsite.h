// SPDX-License-Identifier: MIT
// A trade site with listings on it, for the tests of the trade results.
#pragma once

#include "app/traderesults.h"
#include "core/parser.h"
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "services/ratesstore.h"
#include "services/tradeclient.h"
#include "services/tradesearcher.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

namespace resultsite {

using namespace Qt::Literals::StringLiterals;
using namespace pou;

inline const QString league = u"Runes of Aldur"_s;
inline const QStringList siteLeagues = {league, u"HC Runes of Aldur"_s, u"Standard"_s};

inline QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// x1, x2...
inline QStringList listingIds(int count)
{
    QStringList ids;
    for (int i = 1; i <= count; ++i)
        ids << u"x%1"_s.arg(i);
    return ids;
}

inline QJsonObject price(double amount, const QString &currency)
{
    return {{u"type"_s, u"~price"_s}, {u"amount"_s, amount}, {u"currency"_s, currency}};
}

// A listing the way the site's fetch answers have them, for a made-up helmet.
inline QJsonObject helmet(const QString &id, const QJsonObject &price, const QString &indexed = {},
                          const QString &name = u"Grim Ward"_s)
{
    const QJsonObject account{{u"name"_s, u"Seller%1#1234"_s.arg(id)},
                              {u"online"_s, QJsonObject{{u"league"_s, league}}}};
    const QJsonObject listing{{u"indexed"_s, indexed},
                              {u"account"_s, account},
                              {u"price"_s, price},
                              {u"whisper"_s, u"@Sellerx Hi, I would like to buy your Grim Ward"_s}};
    const QJsonObject item{{u"rarity"_s, u"Rare"_s},
                           {u"name"_s, name},
                           {u"typeLine"_s, u"Iron Hat"_s},
                           {u"ilvl"_s, 80},
                           {u"explicitMods"_s, QJsonArray{u"+80 to maximum Life"_s, u"Cannot be Frozen"_s}}};
    return {{u"id"_s, id}, {u"listing"_s, listing}, {u"item"_s, item}};
}

// The same listing as an instant buyout, which has no whisper.
inline QJsonObject instantBuyout(QJsonObject result)
{
    QJsonObject listing = result.value(u"listing"_s).toObject();
    listing.remove(u"whisper"_s);
    result.insert(u"listing"_s, listing);
    return result;
}

// What the site answers to a fetch of those ids, null for those it has sold.
inline FakeAnswer fetched(const QStringList &ids, const QStringList &sold = {})
{
    QJsonArray result;
    for (const QString &id : ids)
        result << (sold.contains(id) ? QJsonValue(QJsonValue::Null)
                                     : QJsonValue(helmet(id, price(3, u"divine"_s))));
    return {.body = json({{u"result"_s, result}})};
}

// What poe.ninja says currencies are worth, with a Divine Orb at that many
// Exalted Orbs and a Chaos Orb at ten.
inline FakeAnswer ninjaRates(double exalts)
{
    const QJsonObject core{{u"primary"_s, u"divine"_s}, {u"rates"_s, QJsonObject{{u"exalted"_s, exalts}}}};
    const QJsonArray lines{QJsonObject{{u"id"_s, u"divine"_s}, {u"primaryValue"_s, 1}},
                           QJsonObject{{u"id"_s, u"exalted"_s}, {u"primaryValue"_s, 1 / exalts}},
                           QJsonObject{{u"id"_s, u"chaos"_s}, {u"primaryValue"_s, 10 / exalts}}};
    return {.body = json({{u"core"_s, core}, {u"lines"_s, lines}})};
}

inline const Item checked = parseItem(u"Item Class: Helmets\nRarity: Rare\nDoom Visage\nIron Hat\n--------\n"
                                      "Item Level: 80\n--------\n+80 to maximum Life"_s)
                                .value();

// A search that found that many, with a made-up id.
inline TradeFinder::Found foundBy(int total, const QString &searchId = u"Ab3dE9"_s, int ids = -1)
{
    return {.kind = TradeFinder::Kind::Similar,
            .item = checked,
            .title = u"Same stats or better than Doom Visage"_s,
            .league = league,
            .url = QUrl(u"https://www.pathofexile.com/trade2/search/poe2/Runes%20of%20Aldur/"_s + searchId),
            .searchId = searchId,
            .total = total,
            .listingIds = listingIds(ids < 0 ? std::min(total, 100) : ids)};
}

inline QUrl fetchUrl(const QStringList &ids, const QString &searchId = u"Ab3dE9"_s)
{
    return tradeFetchUrl(ids, searchId);
}

// The trade site, poe.ninja, and the results of searches on them.
struct Site
{
    QTemporaryDir dir;
    FakeNetwork network;
    TradeClient client{&network};
    TradeSearcher searcher{&client};
    FakeNetwork ninja;
    RatesStore rates{&ninja, dir.filePath(u"rates"_s)};
    TradeResults results{&searcher, &rates};

    Site()
    {
        searcher.setLeagues(siteLeagues);
        searcher.setSettings(TradeSettings{.league = {.name = league}});
    }

    // after it has asked the site and heard back
    bool loads()
    {
        results.loadMore();
        return results.isBusy() && waitsForIt();
    }

    bool waitsForIt()
    {
        QSignalSpy busy(&results, &TradeResults::busyChanged);
        return results.isBusy() ? busy.wait() && !results.isBusy() : true;
    }

    // so the prices can be worked out
    bool hasRates(double exalts)
    {
        ninja.answers = {ninjaRates(exalts)};
        QSignalSpy refreshed(&rates, &RatesStore::refreshed);
        rates.refresh(league);
        return refreshed.wait() && rates.divineRate() > 0;
    }

    QString text(int row, int role) const { return results.data(results.index(row), role).toString(); }
};

} // namespace resultsite
