// SPDX-License-Identifier: MIT
#include "core/ratelimit.h"

#include <QTest>

using namespace std::chrono_literals;
using namespace pou;

namespace {

const RateLimiter::Clock::time_point start;

} // namespace

class TestRateLimit : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void windows()
    {
        QCOMPARE(rateWindows(u"5:10:60,15:60:300"), (QList<RateWindow>{{5, 10, 60}, {15, 60, 300}}));
        QCOMPARE(rateWindows(u" 8:10:60, 15:60:120 "), (QList<RateWindow>{{8, 10, 60}, {15, 60, 120}}));
        // the ones it can't read are left out
        QCOMPARE(rateWindows(u"5:10,x:10:60,5:0:60,5:10:x,-2:10:60,,7:30:0"),
                 (QList<RateWindow>{{7, 30, 0}}));
        QVERIFY(rateWindows(u"").isEmpty());
    }

    void safeGuesses()
    {
        const RateLimiter limiter;
        QCOMPARE(limiter.limits(), (QList<RateWindow>{{5, 10, 0}, {15, 60, 0}, {30, 300, 0}}));
        QCOMPARE(limiter.wait(start), 0ms);
    }

    void keepsOneSpare()
    {
        RateLimiter limiter;
        for (const auto at : {0s, 1s, 2s})
            limiter.sent(start + at);
        QCOMPARE(limiter.wait(start + 2s), 0ms);
        limiter.sent(start + 3s);
        // the fifth of 5 in 10 seconds waits for the first to be out of them
        QCOMPARE(limiter.wait(start + 3s), 7250ms);
        // and a moment more, as the site counts it from when it got there
        QCOMPARE(limiter.wait(start + 10s), 250ms);
        QCOMPARE(limiter.wait(start + 10250ms), 0ms);
    }

    void longerWindows()
    {
        RateLimiter limiter;
        for (int i = 0; i < 14; ++i)
            limiter.sent(start + i * 4s);
        // the fifteenth of 15 in a minute
        QCOMPARE(limiter.wait(start + 52s), 8250ms);
    }
};

QTEST_GUILESS_MAIN(TestRateLimit)
#include "tst_ratelimit.moc"
