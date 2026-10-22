// SPDX-License-Identifier: MIT
#include "core/ratelimit.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
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

    void learnsTheLimits()
    {
        RateLimiter limiter;
        limiter.learn({.ip = u"8:10:60,15:60:300"_s, .account = u"30:300:1800"_s}, start);
        QCOMPARE(limiter.limits(), (QList<RateWindow>{{8, 10, 60}, {15, 60, 300}, {30, 300, 1800}}));
        // an answer without them keeps them
        limiter.learn({}, start);
        QCOMPARE(limiter.limits().size(), 3);

        for (int i = 0; i < 6; ++i)
            limiter.sent(start);
        QCOMPARE(limiter.wait(start), 0ms);
        limiter.sent(start);
        QCOMPARE(limiter.wait(start), 10250ms);
    }

    void countsRequestsFromElsewhere()
    {
        // four in the last 10 seconds, from the trade site in your browser
        RateLimiter limiter;
        limiter.learn({.ipState = u"4:10:0,4:60:0,4:300:0"_s}, start + 100s);
        QCOMPARE(limiter.wait(start + 100s), 10250ms);

        // the ones in the last minute but not the last 10 seconds went 10
        // seconds ago at the latest
        RateLimiter longer;
        longer.learn({.ipState = u"1:10:0,14:60:0,14:300:0"_s}, start + 100s);
        QCOMPARE(longer.wait(start + 100s), 50000ms);
        RateLimiter unordered;
        unordered.learn({.ipState = u"14:300:0,14:60:0,1:10:0"_s}, start + 100s);
        QCOMPARE(unordered.wait(start + 100s), 50000ms);
    }

    void ownRequestsCountOnce()
    {
        RateLimiter limiter;
        limiter.sent(start);
        limiter.sent(start + 1s);
        limiter.learn({.ipState = u"2:10:0,2:60:0,2:300:0"_s}, start + 1500ms);
        limiter.sent(start + 2s);
        QCOMPARE(limiter.wait(start + 2s), 0ms);
        limiter.sent(start + 3s);
        QCOMPARE(limiter.wait(start + 3s), 7250ms);
    }

    void accountCountsApart()
    {
        // the two the account has on top could have come from another
        // computer just now
        RateLimiter limiter;
        limiter.learn({.ip = u"8:10:60"_s,
                       .ipState = u"1:10:0"_s,
                       .account = u"4:60:60"_s,
                       .accountState = u"3:60:0"_s},
                      start);
        QCOMPARE(limiter.wait(start), 60250ms);

        // and the ones your address had in the minute before the last 10
        // seconds aren't in your account's last 10
        RateLimiter shorter;
        shorter.learn({.ip = u"8:10:60,30:60:300"_s,
                       .ipState = u"1:10:0,10:60:0"_s,
                       .account = u"6:10:60"_s,
                       .accountState = u"5:10:0"_s},
                      start + 100s);
        QCOMPARE(shorter.wait(start + 100s), 10250ms);
    }

    void bans()
    {
        RateLimiter limiter;
        limiter.learn({.ipState = u"6:10:60,6:60:0,6:300:0"_s}, start);
        QCOMPARE(limiter.wait(start), 60000ms);
        QCOMPARE(limiter.wait(start + 30s), 30000ms);
    }

    void retryAfter()
    {
        RateLimiter limiter;
        limiter.learn({.retryAfter = u"30"_s}, start);
        QCOMPARE(limiter.wait(start), 30000ms);
        QCOMPARE(limiter.wait(start + 30s), 0ms);

        // a shorter one later doesn't cut it short
        limiter.learn({.retryAfter = u"30"_s}, start);
        limiter.learn({.retryAfter = u"5"_s}, start + 1s);
        QCOMPARE(limiter.wait(start + 1s), 29000ms);

        RateLimiter unreadable;
        unreadable.learn({.retryAfter = u"soon"_s}, start);
        QCOMPARE(unreadable.wait(start), 0ms);
    }

    void hugeCounts()
    {
        // all the windows are full, as the last 10 seconds are in them
        RateLimiter limiter;
        limiter.learn({.ipState = u"2000000000:10:0"_s}, start);
        QCOMPARE(limiter.wait(start), 300250ms);
    }
};

QTEST_GUILESS_MAIN(TestRateLimit)
#include "tst_ratelimit.moc"
