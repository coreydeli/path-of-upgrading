// SPDX-License-Identifier: MIT
#include "core/budget.h"
#include "core/tradequery.h"
#include "tradefixtures.h"

#include <QTest>

using namespace pou;

namespace {

Budget budget(double min, double max, Currency currency = Currency::Divine, double divineRate = 0)
{
    return {min, max, currency, divineRate};
}

QJsonObject price(double min, double max, const QString &option)
{
    QJsonObject p{{u"option"_s, option}};
    if (min > 0)
        p.insert(u"min"_s, min);
    if (max > 0)
        p.insert(u"max"_s, max);
    return p;
}

// One of a search's filter groups, like "trade_filters".
QJsonObject filtersOf(const QJsonObject &body, QStringView group)
{
    return body.value(u"query")
        .toObject()
        .value(u"filters")
        .toObject()
        .value(group)
        .toObject()
        .value(u"filters")
        .toObject();
}

} // namespace

class TestBudget : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void currencies()
    {
        for (const Currency c : {Currency::Divine, Currency::Exalted, Currency::Chaos})
            QCOMPARE(currencyFromId(currencyId(c)), c);
        QCOMPARE(currencyId(Currency::Exalted), u"exalted"_s);
        QCOMPARE(currencyFromId(u"mirror"), Currency::Divine);
    }

    void prices_data()
    {
        QTest::addColumn<Budget>("given");
        QTest::addColumn<QJsonObject>("expected");

        QTest::newRow("up to") << budget(0, 20) << price(0, 20, u"divine"_s);
        QTest::newRow("big budget") << budget(0, 20, Currency::Divine, 150) << price(0, 20, u"divine"_s);
        QTest::newRow("small budget")
            << budget(0, 1.5, Currency::Divine, 700) << price(0, 1050, u"exalted"_s);
        QTest::newRow("two divines") << budget(0, 2, Currency::Divine, 400) << price(0, 800, u"exalted"_s);
        QTest::newRow("just over two") << budget(0, 2.5, Currency::Divine, 400) << price(0, 2.5, u"divine"_s);
        QTest::newRow("small, no rate") << budget(0, 1.5) << price(0, 1.5, u"divine"_s);
        QTest::newRow("exalts") << budget(0, 500, Currency::Exalted) << price(0, 500, u"exalted"_s);
        QTest::newRow("range") << budget(5, 20, Currency::Divine, 150) << price(5, 20, u"divine"_s);
        QTest::newRow("at least") << budget(10, 0, Currency::Divine, 694) << price(10, 0, u"divine"_s);
        QTest::newRow("at least, no rate") << budget(5, 0) << price(5, 0, u"divine"_s);
        QTest::newRow("at least in exalts") << budget(50, 0, Currency::Exalted) << price(50, 0, u"exalted"_s);
        QTest::newRow("ends swapped") << budget(30, 10, Currency::Chaos) << price(10, 30, u"chaos"_s);
        QTest::newRow("small range") << budget(0.5, 1.5, Currency::Divine, 401)
                                     << price(201, 601, u"exalted"_s);
        QTest::newRow("one price") << budget(1.5, 1.5, Currency::Divine, 401)
                                   << price(601, 601, u"exalted"_s);
        QTest::newRow("no limit") << budget(0, 0) << QJsonObject();
        QTest::newRow("nonsense") << budget(-5, qQNaN(), Currency::Divine, 150) << QJsonObject();
    }

    void prices()
    {
        QFETCH(Budget, given);
        QFETCH(QJsonObject, expected);
        QCOMPARE(priceFilter(given), expected);
    }

    void labels_data()
    {
        QTest::addColumn<Budget>("given");
        QTest::addColumn<QString>("label");

        QTest::newRow("up to") << budget(0, 20) << u"up to 20 div"_s;
        QTest::newRow("range") << budget(5, 20) << u"5 to 20 div"_s;
        QTest::newRow("at least") << budget(5, 0) << u"at least 5 div"_s;
        QTest::newRow("small budget") << budget(0, 1.5, Currency::Divine, 700) << u"up to 1.5 div"_s;
        QTest::newRow("exalts") << budget(0, 500, Currency::Exalted) << u"up to 500 ex"_s;
        QTest::newRow("chaos") << budget(30, 10, Currency::Chaos)
                               << u"10 to 30 chaos (chaos listings only)"_s;
        QTest::newRow("no limit") << budget(0, 0) << QString();
    }

    void labels()
    {
        QFETCH(Budget, given);
        QFETCH(QString, label);
        QCOMPARE(budgetLabel(given), label);
    }

    void appliedToASearch()
    {
        CustomSearchOptions options;
        options.itemClass = u"Helmets"_s;
        const TradeData data{StatIndex::fromJson(tradefixtures::stats()),
                             TradeCategories::fromJson(tradefixtures::filters())};
        TradeQuery q = customSearch({}, data, options);
        applyBudget(q.body, budget(0, 20));
        QCOMPARE(filtersOf(q.body, u"trade_filters"), QJsonObject({{u"price"_s, price(0, 20, u"divine"_s)}}));
        QCOMPARE(filtersOf(q.body, u"type_filters").value(u"category").toObject().value(u"option").toString(),
                 u"armour.helmet"_s);
    }

    void keepsOtherTradeFilters()
    {
        const QJsonObject collapse{{u"option"_s, u"true"_s}};
        const QJsonObject trade{{u"filters"_s, QJsonObject{{u"collapse"_s, collapse}}}};
        QJsonObject body{{u"query"_s, QJsonObject{{u"filters"_s, QJsonObject{{u"trade_filters"_s, trade}}}}}};
        applyBudget(body, budget(5, 0));
        QCOMPARE(filtersOf(body, u"trade_filters"),
                 QJsonObject({{u"collapse"_s, collapse}, {u"price"_s, price(5, 0, u"divine"_s)}}));
    }

    void noBudgetLeavesTheSearchAlone()
    {
        const QJsonObject before{{u"query"_s, QJsonObject{{u"status"_s, u"online"_s}}}};
        QJsonObject body = before;
        applyBudget(body, budget(0, 0));
        QCOMPARE(body, before);
    }
};

QTEST_GUILESS_MAIN(TestBudget)
#include "tst_budget.moc"
