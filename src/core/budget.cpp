// SPDX-License-Identifier: MIT
#include "core/budget.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Up to this many divines, a budget is asked for in exalts when the rate is known.
constexpr double smallDivineBudget = 2;

// Anything but a positive number leaves that end open.
double limitOf(double v)
{
    return std::isfinite(v) && v > 0 ? v : 0;
}

// The budget's ends, lower one first.
std::pair<double, double> limitsOf(const Budget &budget)
{
    double min = limitOf(budget.min);
    double max = limitOf(budget.max);
    if (min > 0 && max > 0 && min > max)
        std::swap(min, max);
    return {min, max};
}

} // namespace

QString currencyId(Currency currency)
{
    switch (currency) {
    case Currency::Divine:
        break;
    case Currency::Exalted:
        return u"exalted"_s;
    case Currency::Chaos:
        return u"chaos"_s;
    }
    return u"divine"_s;
}

Currency currencyFromId(QStringView id)
{
    if (id == u"exalted")
        return Currency::Exalted;
    if (id == u"chaos")
        return Currency::Chaos;
    return Currency::Divine;
}

QJsonObject priceFilter(const Budget &budget)
{
    const auto [min, max] = limitsOf(budget);
    if (min == 0 && max == 0)
        return {};

    QJsonObject price;
    const double rate = budget.divineRate;
    if (budget.currency == Currency::Divine && max > 0 && max <= smallDivineBudget && std::isfinite(rate) &&
        rate > 0) {
        // rounded inwards, so it stays inside the budget
        const double maxExalts = std::floor(max * rate);
        if (min > 0)
            price.insert(u"min"_s, std::min(std::ceil(min * rate), maxExalts));
        price.insert(u"max"_s, maxExalts);
        price.insert(u"option"_s, currencyId(Currency::Exalted));
        return price;
    }
    if (min > 0)
        price.insert(u"min"_s, min);
    if (max > 0)
        price.insert(u"max"_s, max);
    price.insert(u"option"_s, currencyId(budget.currency));
    return price;
}

void applyBudget(QJsonObject &body, const Budget &budget)
{
    const QJsonObject price = priceFilter(budget);
    if (price.isEmpty())
        return;
    QJsonObject query = body.value(u"query").toObject();
    QJsonObject filters = query.value(u"filters").toObject();
    QJsonObject trade = filters.value(u"trade_filters").toObject();
    QJsonObject tradeFilters = trade.value(u"filters").toObject();
    tradeFilters.insert(u"price"_s, price);
    trade.insert(u"filters"_s, tradeFilters);
    filters.insert(u"trade_filters"_s, trade);
    query.insert(u"filters"_s, filters);
    body.insert(u"query"_s, query);
}

} // namespace pou
