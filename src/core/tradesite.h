// SPDX-License-Identifier: MIT
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace pou {

// Addresses on the official trade site. Leagues are named as the site names
// them: "Standard", "Runes of Aldur", "HC Runes of Aldur".

// One of the site's data endpoints: "stats", "filters" or "leagues".
QUrl tradeDataUrl(const QString &name);

// Where a search body is POSTed. The answer has the search's id, how many
// listings it found and the ids of the first 100.
QUrl tradeSearchUrl(const QString &league);

// The listings themselves, a few at a time.
inline constexpr int tradeFetchLimit = 10;
QUrl tradeFetchUrl(const QStringList &listingIds, const QString &searchId);

// A search on the site, to open in the browser.
QUrl tradeResultsUrl(const QString &league, const QString &searchId);

// A search the site runs when the page opens, for when sending it from the
// app didn't work.
QUrl tradePrefilledUrl(const QString &league, const QJsonObject &body);

} // namespace pou
