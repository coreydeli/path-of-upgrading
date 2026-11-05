// SPDX-License-Identifier: MIT
#include "services/ratesrefresher.h"

#include "core/settings.h"
#include "services/ratesstore.h"

#include <algorithm>

namespace pou {
namespace {

// Six doublings take any wait to the longest.
constexpr int mostFailures = 6;

} // namespace

RatesRefresher::RatesRefresher(RatesStore *rates, QObject *parent)
    : QObject(parent)
    , m_rates(rates)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &RatesRefresher::ask);
    connect(rates, &RatesStore::refreshed, this, &RatesRefresher::answered);
}

void RatesRefresher::follow(const QString &league, int minutes)
{
    if (league.isEmpty()) {
        stop();
        return;
    }
    minutes = std::clamp(minutes, minRateMinutes, maxRateMinutes);
    if (m_running && league == m_league && minutes == m_minutes)
        return;
    m_running = true;
    m_league = league;
    m_minutes = minutes;
    m_failures = 0;
    m_asked = false;
    arm();
}

void RatesRefresher::stop()
{
    m_timer.stop();
    m_running = false;
    m_asked = false;
    m_failures = 0;
}

int RatesRefresher::waitMinutes() const
{
    return std::min(m_minutes << m_failures, maxRateMinutes);
}

void RatesRefresher::arm()
{
    m_timer.start(waitMinutes() * m_minute);
}

void RatesRefresher::ask()
{
    // that one's answer will do, and a try has to wait for it
    if (m_rates->isRefreshing()) {
        arm();
        return;
    }
    m_asked = true;
    m_rates->refresh(m_league, true);
}

void RatesRefresher::answered()
{
    // the store also refreshes for searches, which aren't ours to judge
    if (!m_asked)
        return;
    m_asked = false;
    m_failures = m_rates->errorString().isEmpty() ? 0 : std::min(m_failures + 1, mostFailures);
    arm();
}

} // namespace pou
