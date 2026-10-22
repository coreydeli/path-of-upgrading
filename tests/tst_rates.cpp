// SPDX-License-Identifier: MIT
#include "core/rates.h"

#include <QJsonArray>
#include <QTest>

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
        QCOMPARE(currencyRatesInExalts(data).value(u"divine"_s), 150.0);
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

    void allCurrencies()
    {
        const CurrencyRates rates = currencyRatesInExalts(
            overview(primary(u"exalted"_s), {line(u"divine"_s, 700), line(u"chaos"_s, 12.5)}));
        QCOMPARE(rates, CurrencyRates({{u"exalted"_s, 1}, {u"divine"_s, 700}, {u"chaos"_s, 12.5}}));
    }

    void urls()
    {
        const QList<QUrl> urls = ninjaCurrencyUrls(u"Rise of the Abyssal"_s);
        QCOMPARE(urls.size(), 2);
        QCOMPARE(urls.at(0).toString(QUrl::FullyEncoded),
                 u"https://poe.ninja/poe2/api/economy/exchange/current/overview"
                 "?league=Rise%20of%20the%20Abyssal&type=Currency"_s);
        QCOMPARE(urls.at(1).toString(QUrl::FullyEncoded),
                 u"https://poe.ninja/poe2/api/economy/currencyexchange/overview"
                 "?leagueName=Rise%20of%20the%20Abyssal&overviewName=Currency"_s);
    }
};

QTEST_GUILESS_MAIN(TestRates)
#include "tst_rates.moc"
