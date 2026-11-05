// SPDX-License-Identifier: MIT
#include "app/tradeoptions.h"

#include "app/settingsstore.h"
#include "core/rates.h"
#include "services/ratesstore.h"
#include "services/tradedatastore.h"

#include <algorithm>
#include <cmath>
#include <optional>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Neither a budget nor a rate goes below nothing.
std::optional<double> amount(double value)
{
    if (!std::isfinite(value))
        return std::nullopt;
    return std::max(0.0, value);
}

} // namespace

TradeOptions::TradeOptions(SettingsStore *settings, TradeDataStore *data, RatesStore *rates, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_data(data)
    , m_rates(rates)
{
    connect(settings, &SettingsStore::changed, this, &TradeOptions::changed);
    connect(data, &TradeDataStore::refreshed, this, &TradeOptions::changed);
    connect(rates, &RatesStore::refreshed, this, &TradeOptions::changed);
    // the status also goes by the settings and the minutes
    connect(settings, &SettingsStore::changed, this, &TradeOptions::ratesStatusChanged);
    connect(rates, &RatesStore::refreshed, this, &TradeOptions::ratesStatusChanged);
    connect(data, &TradeDataStore::refreshed, this, &TradeOptions::ratesStatusChanged);
    connect(&m_tick, &QTimer::timeout, this, &TradeOptions::ratesStatusChanged);
    m_tick.setInterval(std::chrono::minutes(1));
    m_tick.start();
}

const TradeSettings &TradeOptions::trade() const
{
    return m_settings->settings().trade;
}

League TradeOptions::searched() const
{
    return resolveLeague(trade().league, m_data->leagues());
}

template<typename Edit>
void TradeOptions::change(Edit edit)
{
    Settings settings = m_settings->settings();
    edit(settings.trade);
    // which does nothing when it's the same
    m_settings->set(settings);
}

void TradeOptions::setEnabled(bool enabled)
{
    change([&](TradeSettings &trade) { trade.enabled = enabled; });
}

void TradeOptions::setLeague(const QString &league)
{
    change([&](TradeSettings &trade) { trade.league.name = league.trimmed(); });
}

void TradeOptions::setHardcore(bool hardcore)
{
    change([&](TradeSettings &trade) { trade.league.hardcore = hardcore; });
}

void TradeOptions::setSoloSelfFound(bool soloSelfFound)
{
    change([&](TradeSettings &trade) { trade.league.soloSelfFound = soloSelfFound; });
}

QString TradeOptions::sellers() const
{
    return listingStatusId(trade().status);
}

void TradeOptions::setSellers(const QString &sellers)
{
    change([&](TradeSettings &trade) { trade.status = listingStatusFromId(sellers); });
}

void TradeOptions::setBudgetMin(double min)
{
    if (const std::optional<double> value = amount(min))
        change([&](TradeSettings &trade) { trade.budget.min = *value; });
}

void TradeOptions::setBudgetMax(double max)
{
    if (const std::optional<double> value = amount(max))
        change([&](TradeSettings &trade) { trade.budget.max = *value; });
}

QString TradeOptions::currency() const
{
    return currencyId(trade().budget.currency);
}

void TradeOptions::setCurrency(const QString &currency)
{
    change([&](TradeSettings &trade) { trade.budget.currency = currencyFromId(currency); });
}

void TradeOptions::setDivineRate(double rate)
{
    if (const std::optional<double> value = amount(rate))
        change([&](TradeSettings &trade) { trade.budget.divineRate = *value; });
}

void TradeOptions::setAutoRate(bool autoRate)
{
    change([&](TradeSettings &trade) { trade.autoRate = autoRate; });
}

void TradeOptions::setRateMinutes(int minutes)
{
    change([&](TradeSettings &trade) {
        trade.rateMinutes = std::clamp(minutes, minRateMinutes, maxRateMinutes);
    });
}

QStringList TradeOptions::leagues() const
{
    const QString standard = u"Standard"_s;
    QStringList names = leagueNames(m_data->leagues());
    names.removeOne(standard);
    if (const QString yours = league(); !yours.isEmpty() && yours != standard && !names.contains(yours))
        names << yours;
    // last, as the site has it
    names << standard;
    return names;
}

QString TradeOptions::searchedLeague() const
{
    return searched().displayName();
}

QString TradeOptions::unavailable() const
{
    return tradeUnavailable(trade());
}

double TradeOptions::ninjaRate() const
{
    // the rates are for the league last asked about, which is the searched
    // one once the app has caught up
    return m_rates->league() == searched().tradeId() ? m_rates->divineRate() : 0;
}

QString TradeOptions::ratesStatus() const
{
    if (!trade().autoRate || !unavailable().isEmpty())
        return {};
    // the rates are the searched league's, once the app has caught up
    if (m_rates->league() != searched().tradeId())
        return pou::ratesStatus({}, {}, m_now());
    return pou::ratesStatus(m_rates->fetched(), m_rates->errorString(), m_now());
}

} // namespace pou
