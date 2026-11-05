// SPDX-License-Identifier: MIT
#include "core/rates.h"

#include <QJsonArray>
#include <QTest>
#include <QTimeZone>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

QJsonObject line(const QString &id, double primaryValue)
{
    return {{u"id"_s, id}, {u"primaryValue"_s, primaryValue}};
}

QJsonObject currency(const QString &id, const QString &name)
{
    return {{u"id"_s, id}, {u"name"_s, name}};
}

QJsonObject overview(const QJsonObject &core, const QJsonArray &lines)
{
    return {{u"core"_s, core}, {u"lines"_s, lines}};
}

QJsonObject primary(const QString &id)
{
    return {{u"primary"_s, id}};
}

// 0 when there's no rate
double rateOf(const QJsonObject &overview)
{
    return divineInExalts(overview).value_or(0);
}

} // namespace

class TestRates : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pricedInExalts()
    {
        QJsonObject core = primary(u"exalted"_s);
        core.insert(u"rates"_s, QJsonObject{{u"divine"_s, 0.0066}});
        core.insert(u"items"_s, QJsonArray{currency(u"divine"_s, u"Divine Orb"_s),
                                           currency(u"exalted"_s, u"Exalted Orb"_s)});
        QCOMPARE(rateOf(overview(core, {line(u"divine"_s, 152.3), line(u"chaos"_s, 8)})), 152.3);
    }

    void onlyRates()
    {
        QJsonObject core = primary(u"exalted"_s);
        core.insert(u"rates"_s, QJsonObject{{u"divine"_s, 0.005}});
        QCOMPARE(rateOf(overview(core, {})), 200.0);
    }

    // what poe.ninja answered in October 2026
    void pricedInDivines()
    {
        QJsonObject core = primary(u"divine"_s);
        core.insert(u"rates"_s, QJsonObject{{u"exalted"_s, 555.7}, {u"chaos"_s, 9.75}});
        const QJsonObject data =
            overview(core, {line(u"exalted"_s, 0.0018), line(u"chaos"_s, 0.1025), line(u"divine"_s, 1)});
        QCOMPARE(rateOf(data), 555.6);
        const CurrencyRates rates = currencyRatesInExalts(data);
        QCOMPARE(rates.value(u"divine"_s), 555.5556);
        QCOMPARE(rates.value(u"chaos"_s), 56.9444);
        QCOMPARE(rates.value(u"exalted"_s), 1.0);
    }

    void olderShape()
    {
        QJsonObject data =
            overview(QJsonObject{{u"version"_s, 1}}, {line(u"divine"_s, 30), line(u"exalted"_s, 0.2)});
        data.insert(u"items"_s, QJsonArray{currency(u"divine"_s, u"Divine Orb"_s),
                                           currency(u"exalted"_s, u"Exalted Orb"_s)});
        QCOMPARE(rateOf(data), 150.0);
        QCOMPARE(currencyRatesInExalts(data), CurrencyRates({{u"exalted"_s, 1}, {u"divine"_s, 150}}));
    }

    void idsByName()
    {
        QJsonObject core = primary(u"ex"_s);
        core.insert(u"items"_s,
                    QJsonArray{currency(u"div2"_s, u"Divine Orb"_s), currency(u"ex"_s, u"Exalted Orb"_s)});
        QCOMPARE(rateOf(overview(core, {line(u"div2"_s, 99)})), 99.0);
    }

    void nonsense()
    {
        QVERIFY(!divineInExalts({}));
        QVERIFY(!divineInExalts(overview({}, {})));
        // a Divine Orb worth half an exalt
        QVERIFY(!divineInExalts(overview(primary(u"exalted"_s), {line(u"divine"_s, 0.5)})));
        QVERIFY(currencyRatesInExalts(overview(primary(u"chaos"_s), {line(u"divine"_s, 30)})).isEmpty());
    }

    void believableRates()
    {
        const auto worth = [](double divine) {
            return rateOf(overview(primary(u"exalted"_s), {line(u"divine"_s, divine)}));
        };
        QCOMPARE(worth(1), 1.0);
        QCOMPARE(worth(100000), 100000.0);
        QCOMPARE(worth(0.9), 0.0);
        QCOMPARE(worth(100001), 0.0);
    }

    void needsBothCurrencies()
    {
        QVERIFY(!divineInExalts(overview(primary(u"chaos"_s), {line(u"divine"_s, 30)})));
        QVERIFY(!divineInExalts(overview(primary(u"chaos"_s), {line(u"exalted"_s, 0.2)})));
    }

    void ratesLeaveOutWhatItCantUse()
    {
        const CurrencyRates rates = currencyRatesInExalts(overview(
            primary(u"exalted"_s), {line(u"divine"_s, 700), line(u"mirror"_s, 0), line(QString(), 5)}));
        QCOMPARE(rates, CurrencyRates({{u"exalted"_s, 1}, {u"divine"_s, 700}}));
    }

    void primaryWithoutALine()
    {
        const QJsonObject data = overview(primary(u"divine"_s), {line(u"exalted"_s, 0.0018)});
        QCOMPARE(currencyRatesInExalts(data), CurrencyRates({{u"exalted"_s, 1}, {u"divine"_s, 555.5556}}));
    }

    void allCurrencies()
    {
        const CurrencyRates rates = currencyRatesInExalts(
            overview(primary(u"exalted"_s), {line(u"divine"_s, 700), line(u"chaos"_s, 12.5)}));
        QCOMPARE(rates, CurrencyRates({{u"exalted"_s, 1}, {u"divine"_s, 700}, {u"chaos"_s, 12.5}}));
    }

    void url()
    {
        QCOMPARE(ninjaCurrencyUrl(u"HC Runes of Aldur"_s).toString(QUrl::FullyEncoded),
                 u"https://poe.ninja/poe2/api/economy/exchange/current/overview"
                 "?league=HC%20Runes%20of%20Aldur&type=Currency"_s);
    }

    void timeAgo_data()
    {
        QTest::addColumn<qint64>("seconds");
        QTest::addColumn<QString>("said");
        QTest::newRow("ahead of the clock") << qint64(-90) << u"just now"_s;
        QTest::newRow("now") << qint64(0) << u"just now"_s;
        QTest::newRow("almost a minute") << qint64(59) << u"just now"_s;
        QTest::newRow("a minute") << qint64(60) << u"1 minute ago"_s;
        QTest::newRow("minutes") << qint64(7 * 60 + 30) << u"7 minutes ago"_s;
        QTest::newRow("almost an hour") << qint64(59 * 60 + 59) << u"59 minutes ago"_s;
        QTest::newRow("an hour") << qint64(3600) << u"1 hour ago"_s;
        QTest::newRow("hours") << qint64(5 * 3600) << u"5 hours ago"_s;
        QTest::newRow("almost two days") << qint64(47 * 3600) << u"47 hours ago"_s;
        QTest::newRow("days") << qint64(3 * 24 * 3600) << u"3 days ago"_s;
    }

    void timeAgo()
    {
        QFETCH(qint64, seconds);
        QFETCH(QString, said);
        const QDateTime now(QDate(2026, 11, 4), QTime(12, 0), QTimeZone::UTC);
        QCOMPARE(pou::timeAgo(now.addSecs(-seconds), now), said);
    }

    void statusOfTheRates()
    {
        const QDateTime now(QDate(2026, 11, 4), QTime(12, 0), QTimeZone::UTC);
        const QDateTime fiveAgo = now.addSecs(-5 * 60);
        QCOMPARE(ratesStatus(fiveAgo, {}, now), u"Prices updated 5 minutes ago."_s);
        QCOMPARE(ratesStatus(now, {}, now), u"Prices updated just now."_s);
        QCOMPARE(ratesStatus({}, {}, now), u"The prices haven't come in yet."_s);
        // when it let them down, with the reason and what there is
        QCOMPARE(ratesStatus(fiveAgo, u"poe.ninja didn't answer in time."_s, now),
                 u"poe.ninja didn't answer in time. Last updated 5 minutes ago."_s);
        QCOMPARE(ratesStatus({}, u"Couldn't reach poe.ninja: Host poe.ninja not found"_s, now),
                 u"Couldn't reach poe.ninja: Host poe.ninja not found."_s);
    }
};

QTEST_GUILESS_MAIN(TestRates)
#include "tst_rates.moc"
