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
    Clock::duration wait = std::max(m_blockedUntil, now) - now;
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

void RateLimiter::learn(const RateHeaders &headers, Clock::time_point now)
{
    if (const QList<RateWindow> limits = rateWindows(headers.ip) + rateWindows(headers.account);
        !limits.isEmpty())
        m_limits = limits;
    // apart, as your account's requests can come from elsewhere and your
    // address's from other accounts
    takeCounts(rateWindows(headers.ipState), now);
    takeCounts(rateWindows(headers.accountState), now);

    bool ok = false;
    if (const int seconds = headers.retryAfter.toInt(&ok); ok && seconds > 0)
        m_blockedUntil = std::max(m_blockedUntil, now + std::chrono::seconds(seconds));
    forgetOld(now);
}

void RateLimiter::note(Clock::time_point at)
{
    m_sent.insert(std::upper_bound(m_sent.cbegin(), m_sent.cend(), at), at);
}

// Adds the requests the site counted in a -state header that aren't in the
// log already.
void RateLimiter::takeCounts(QList<RateWindow> counted, Clock::time_point now)
{
    std::sort(counted.begin(), counted.end(),
              [](const RateWindow &a, const RateWindow &b) { return a.seconds < b.seconds; });
    // More than any limit allows makes no difference to the wait, and a
    // header that says millions would fill the memory.
    int most = 0;
    for (const RateWindow &limit : std::as_const(m_limits))
        most = std::max(most, limit.hits);

    // The site doesn't say when they went, only in which windows. The ones
    // that aren't in a shorter window went before it, and taking them as
    // going just then waits as long as they could need.
    Clock::time_point at = now;
    for (const RateWindow &window : std::as_const(counted)) {
        for (qsizetype n = sentWithin(now, window.seconds); n < std::min(window.hits, most); ++n)
            note(at);
        at = now - countedFor(window.seconds);
        if (window.ban > 0)
            m_blockedUntil = std::max(m_blockedUntil, now + std::chrono::seconds(window.ban));
    }
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
