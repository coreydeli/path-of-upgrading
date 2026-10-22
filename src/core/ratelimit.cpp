// SPDX-License-Identifier: MIT
#include "core/ratelimit.h"

#include <algorithm>

using namespace std::chrono_literals;

namespace pou {
namespace {

using Clock = RateLimiter::Clock;

// How long a request counts in a window: a little longer than the window, as
// the site counts it from when it gets there.
constexpr Clock::duration countedFor(int seconds)
{
    return std::chrono::seconds(seconds) + 250ms;
}

// The number, or -1 when there's none.
int number(QStringView text)
{
    bool ok = false;
    const int n = text.toInt(&ok);
    return ok ? n : -1;
}

} // namespace

QList<RateWindow> rateWindows(QStringView header)
{
    QList<RateWindow> windows;
    for (QStringView part : header.split(u',', Qt::SkipEmptyParts)) {
        const QList<QStringView> numbers = part.split(u':');
        if (numbers.size() != 3)
            continue;
        const RateWindow window{number(numbers[0]), number(numbers[1]), number(numbers[2])};
        if (window.hits >= 0 && window.seconds > 0 && window.ban >= 0)
            windows << window;
    }
    return windows;
}

RateLimiter::RateLimiter()
    : m_limits{{5, 10, 0}, {15, 60, 0}, {30, 300, 0}}
{}

std::chrono::milliseconds RateLimiter::wait(Clock::time_point now) const
{
    Clock::duration wait{};
    for (const RateWindow &limit : m_limits) {
        const qsizetype room = std::max(1, limit.hits - 1);
        if (sentWithin(now, limit.seconds) < room)
            continue;
        // there's room again once this one stops counting
        const Clock::time_point leaving = m_sent.at(m_sent.size() - room);
        wait = std::max(wait, leaving + countedFor(limit.seconds) - now);
    }
    return std::chrono::ceil<std::chrono::milliseconds>(wait);
}

void RateLimiter::sent(Clock::time_point now)
{
    note(now);
    forgetOld(now);
}

void RateLimiter::note(Clock::time_point at)
{
    m_sent.insert(std::upper_bound(m_sent.cbegin(), m_sent.cend(), at), at);
}

qsizetype RateLimiter::sentWithin(Clock::time_point now, int seconds) const
{
    const Clock::time_point since = now - countedFor(seconds);
    return m_sent.cend() - std::upper_bound(m_sent.cbegin(), m_sent.cend(), since);
}

void RateLimiter::forgetOld(Clock::time_point now)
{
    int longest = 0;
    for (const RateWindow &limit : std::as_const(m_limits))
        longest = std::max(longest, limit.seconds);
    const Clock::time_point since = now - countedFor(longest);
    m_sent.erase(m_sent.cbegin(), std::upper_bound(m_sent.cbegin(), m_sent.cend(), since));
}

} // namespace pou
