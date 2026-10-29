// SPDX-License-Identifier: MIT
#include "app/tradereport.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

class TestTradeReport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void searching()
    {
        const TradeReport report = searchingReport();
        QVERIFY(report.asked);
        QVERIFY(report.busy);
        QVERIFY(!report.failed);
        QVERIFY(!report.listed);
        QCOMPARE(report.status, u"Searching the trade site"_s);
        QVERIFY(report.note.isEmpty());
    }

    void waiting_data()
    {
        QTest::addColumn<std::chrono::milliseconds>("wait");
        QTest::addColumn<QString>("status");

        QTest::newRow("seconds") << 8200ms << u"Waiting 9 seconds for the trade site"_s;
        QTest::newRow("one") << std::chrono::milliseconds(1s) << u"Waiting a second for the trade site"_s;
        QTest::newRow("hardly any") << 1ms << u"Waiting a second for the trade site"_s;
    }

    void waiting()
    {
        QFETCH(std::chrono::milliseconds, wait);
        QFETCH(QString, status);

        const TradeReport report = searchingReport(wait);
        QVERIFY(report.busy);
        QCOMPARE(report.status, status);
        QCOMPARE(report.note, u"It takes only a few searches at a time."_s);
    }

    void found()
    {
        const TradeFinder::Found found{
            .title = u"Same stats or better than Doom Visage"_s,
            .note = u"Cheapest first."_s,
            .league = u"HC Runes of Aldur"_s,
            .url = QUrl(u"https://www.pathofexile.com/trade2/search/poe2/HC%20Runes%20of%20Aldur/Ab3dE9"_s),
            .total = 1234,
        };
        const TradeReport report = reportOf(found);
        QCOMPARE(report, (TradeReport{.asked = true,
                                      .listed = true,
                                      .title = found.title,
                                      .status = u"1,234 listed in HC Runes of Aldur"_s,
                                      .note = found.note,
                                      .url = found.url}));
    }

    void noneListed()
    {
        TradeFinder::Found found{.league = u"Runes of Aldur"_s, .total = 0};
        QCOMPARE(reportOf(found).status, u"None listed in Runes of Aldur right now"_s);
        QVERIFY(!reportOf(found).listed);
        found.total = 1;
        QCOMPARE(reportOf(found).status, u"1 listed in Runes of Aldur"_s);
        QVERIFY(reportOf(found).listed);
    }

    void failed()
    {
        const QString why = u"Trade searches are turned off in the settings."_s;
        QCOMPARE(failedReport(why), (TradeReport{.asked = true, .failed = true, .status = why}));
    }
};

QTEST_GUILESS_MAIN(TestTradeReport)
#include "tst_tradereport.moc"
