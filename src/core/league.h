// SPDX-License-Identifier: MIT
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace pou {

// The league you play in. It decides where trade searches and prices come
// from, and whether you can trade at all.
struct League
{
    QString name; // without hardcore or SSF: "Runes of Aldur", "Standard"
    bool hardcore = false;
    bool soloSelfFound = false;

    bool canTrade() const { return !soloSelfFound; }
    // As the trade site and poe.ninja name it: "HC Runes of Aldur", or
    // "Hardcore" for hardcore Standard. Empty for SSF, which has no trading.
    QString tradeId() const;
    // "Hardcore SSF Runes of Aldur"
    QString displayName() const;

    bool operator==(const League &) const = default;
};

// Reads a league's name the way the game, the trade site or poe.ninja write
// it: "HC Runes of Aldur", "Hardcore", "SSF Standard", "Solo Self-Found
// Runes of Aldur".
League leagueFromName(QStringView name);

// The league ids on the trade site's /api/trade2/data/leagues, in its order.
QStringList tradeLeaguesFromJson(const QJsonObject &response);

// The leagues to pick from, without their hardcore versions: the site's
// current leagues in its order, then Standard.
QStringList leagueNames(const QStringList &tradeLeagues);

// The league to use for what you set. An empty name follows the current
// league, the first the site lists, and so does a league that has ended.
// Without the site's list (offline, say) your league is used as it is, and
// following the current one falls back to Standard.
League resolveLeague(const League &wanted, const QStringList &tradeLeagues);

} // namespace pou
