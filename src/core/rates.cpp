// SPDX-License-Identifier: MIT
#include "core/rates.h"

#include "core/numbers.h"

#include <QJsonArray>
#include <QUrlQuery>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QUrl ninjaUrl(const QString &path, const QList<std::pair<QString, QString>> &query)
{
    QUrl url(u"https://poe.ninja/poe2/api/economy/"_s + path);
    QUrlQuery q;
    q.setQueryItems(query);
    url.setQuery(q);
    return url;
}

QJsonArray linesOf(const QJsonObject &overview)
{
    return overview.value(u"lines").toArray();
}

QJsonObject lineFor(const QJsonObject &overview, const QString &id)
{
    for (const QJsonValue &line : linesOf(overview)) {
        if (line.toObject().value(u"id").toString() == id)
            return line.toObject();
    }
    return {};
}

// A currency's id in the overview, by its name, or the usual one.
QString idOf(const QJsonObject &overview, QStringView name, const QString &fallback)
{
    const QJsonObject core = overview.value(u"core").toObject();
    const QJsonValue coreItems = core.value(u"items");
    const QJsonArray items = coreItems.isArray() ? coreItems.toArray() : overview.value(u"items").toArray();
    for (const QJsonValue &v : items) {
        const QJsonObject item = v.toObject();
        const QString id = item.value(u"id").toString();
        if (!id.isEmpty() && item.value(u"name").toString().compare(name, Qt::CaseInsensitive) == 0)
            return id;
    }
    return fallback;
}

// What one of a currency is worth in the overview's primary currency.
std::optional<double> primaryValue(const QJsonObject &overview, const QString &id)
{
    if (const double v = lineFor(overview, id).value(u"primaryValue").toDouble(); v > 0)
        return v;
    const QJsonObject core = overview.value(u"core").toObject();
    if (core.value(u"primary").toString() == id)
        return 1;
    // how many of it one of the primary currency gets you
    if (const double rate = core.value(u"rates").toObject().value(id).toDouble(); rate > 0)
        return 1 / rate;
    return std::nullopt;
}

} // namespace

QList<QUrl> ninjaCurrencyUrls(const QString &league)
{
    return {
        ninjaUrl(u"exchange/current/overview"_s, {{u"league"_s, league}, {u"type"_s, u"Currency"_s}}),
        ninjaUrl(u"currencyexchange/overview"_s,
                 {{u"leagueName"_s, league}, {u"overviewName"_s, u"Currency"_s}}),
    };
}

std::optional<double> divineInExalts(const QJsonObject &overview)
{
    const auto divine = primaryValue(overview, idOf(overview, u"Divine Orb", u"divine"_s));
    const auto exalted = primaryValue(overview, idOf(overview, u"Exalted Orb", u"exalted"_s));
    if (!divine || !exalted)
        return std::nullopt;
    const double rate = *divine / *exalted;
    // a Divine Orb has always been worth well over one Exalted Orb
    if (!std::isfinite(rate) || rate < 1 || rate > 100000)
        return std::nullopt;
    return roundTo(rate, 1);
}

CurrencyRates currencyRatesInExalts(const QJsonObject &overview)
{
    const QString primary = overview.value(u"core").toObject().value(u"primary").toString();
    double exalted = lineFor(overview, u"exalted"_s).value(u"primaryValue").toDouble();
    if (!(exalted > 0)) {
        // without a line for it, exalts have to be what everything is priced in
        if (!primary.isEmpty() && primary != u"exalted")
            return {};
        exalted = 1;
    }

    CurrencyRates rates{{u"exalted"_s, 1}};
    for (const QJsonValue &v : linesOf(overview)) {
        const QJsonObject line = v.toObject();
        const QString id = line.value(u"id").toString();
        const double value = line.value(u"primaryValue").toDouble();
        if (!id.isEmpty() && value > 0)
            rates.insert(id, roundTo(value / exalted, 4));
    }
    if (!primary.isEmpty() && !rates.contains(primary))
        rates.insert(primary, roundTo(1 / exalted, 4));
    return rates;
}

} // namespace pou
