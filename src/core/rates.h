// SPDX-License-Identifier: MIT
#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <optional>

namespace pou {

// Currency prices from poe.ninja's PoE2 currency exchange overview. Its
// older answers, from the /currencyexchange/overview it no longer has, came
// in another shape, and that's read too.

// Where to ask for a league's. poe.ninja names leagues as the trade site does.
QUrl ninjaCurrencyUrl(const QString &league);

// How many Exalted Orbs a Divine Orb is worth, to one decimal. Empty when
// the overview doesn't say, or says something no market has ever had.
std::optional<double> divineInExalts(const QJsonObject &overview);

// What each currency in the overview is worth in Exalted Orbs, by the ids
// the trade site prices listings in ("divine", "chaos"). Empty when the
// overview has no way to tell.
using CurrencyRates = QHash<QString, double>;
CurrencyRates currencyRatesInExalts(const QJsonObject &overview);

// How long ago, as people say it: "just now", "1 minute ago", "3 hours ago".
// A time ahead of the clock is just now.
QString timeAgo(const QDateTime &then, const QDateTime &now);

// What the settings window says of the rates: "Rates updated 3 minutes
// ago.", the reason they couldn't be updated (with when they last were, if
// ever), or that none have come in. The reason is empty when the last try
// worked.
QString ratesStatus(const QDateTime &fetched, const QString &error, const QDateTime &now);

} // namespace pou
