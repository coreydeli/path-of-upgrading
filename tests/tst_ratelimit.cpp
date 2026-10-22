// SPDX-License-Identifier: MIT
#include "core/ratelimit.h"

#include <QTest>

using namespace pou;

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
};

QTEST_GUILESS_MAIN(TestRateLimit)
#include "tst_ratelimit.moc"
