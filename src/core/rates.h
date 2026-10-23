// SPDX-License-Identifier: MIT
#pragma once

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

} // namespace pou
