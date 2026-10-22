// SPDX-License-Identifier: MIT
#pragma once

#include <QJsonObject>
#include <QString>

namespace pou {

// What you're willing to pay, as a price filter on every trade search.

enum class Currency { Divine, Exalted, Chaos };

QString currencyId(Currency currency); // "divine", as the trade site calls it
// Divine for anything unknown.
Currency currencyFromId(QStringView id);

struct Budget
{
    // Either end can be left open with 0.
    double min = 0;
    double max = 0;
    Currency currency = Currency::Divine;
    // Exalted Orbs to a Divine Orb at today's market rate, 0 when unknown.
    double divineRate = 0;

    friend bool operator==(const Budget &, const Budget &) = default;
};

// The search's price filter, {min, max, option}. Empty when there's no limit.
//
// It always asks for a currency rather than the site's "Exalted Orb
// equivalent", which converts with the site's own rate, far from the
// market's. Small divine budgets are asked for in exalts at the market rate,
// as most items at those prices are listed in exalts.
QJsonObject priceFilter(const Budget &budget);

// Adds the budget's price filter to a search body.
void applyBudget(QJsonObject &body, const Budget &budget);

// What the budget asks for, for a search's summary: "5 to 20 div",
// "up to 20 div", "at least 5 div". Empty when there's no limit.
QString budgetLabel(const Budget &budget);

} // namespace pou
