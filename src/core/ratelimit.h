// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QString>
#include <chrono>

namespace pou {

// The trade site's request limits. Its answers give them in x-rate-limit-ip
// and, when you're signed in, x-rate-limit-account, and searches have limits
// apart from fetching listings.

// One of the limits: "5:10:60" is 5 requests in 10 seconds, with a ban of 60
// seconds for more. In a -state header the numbers are the requests made in
// that window so far and how long a ban has left to run.
struct RateWindow
{
    int hits = 0;
    int seconds = 0;
    int ban = 0;

    friend bool operator==(const RateWindow &, const RateWindow &) = default;
};

// The windows in a header, "5:10:60,15:60:300", leaving out any it can't read.
QList<RateWindow> rateWindows(QStringView header);

// Spaces requests out to stay under the limits with one to spare in each, as
// going over gets you banned for minutes.
class RateLimiter
{
public:
    using Clock = std::chrono::steady_clock;

    // Safe guesses until the site says.
    RateLimiter();

    // How long until the next request can go, zero for now.
    std::chrono::milliseconds wait(Clock::time_point now) const;
    void sent(Clock::time_point now);

    QList<RateWindow> limits() const { return m_limits; }

private:
    void note(Clock::time_point at);
    qsizetype sentWithin(Clock::time_point now, int seconds) const;
    void forgetOld(Clock::time_point now);

    QList<RateWindow> m_limits;
    QList<Clock::time_point> m_sent; // oldest first
};

} // namespace pou
