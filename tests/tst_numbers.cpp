// SPDX-License-Identifier: MIT
#include "core/numbers.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestNumbers : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void halvesRoundUp()
    {
        QCOMPARE(roundHalfUp(2.5), 3.0);
        QCOMPARE(roundHalfUp(-2.5), -2.0);
        QCOMPARE(roundHalfUp(-2.6), -3.0);
        QCOMPARE(roundHalfUp(0.49999999999999994), 0.0);
        QCOMPARE(roundTo(1.25, 1), 1.3);
        QCOMPARE(roundTo(143.00000000000003, 1), 143.0);
        QCOMPARE(roundTo(2.0 / 3.0, 2), 0.67);
    }

    void printing_data()
    {
        QTest::addColumn<double>("value");
        QTest::addColumn<QString>("text");
        QTest::newRow("integer") << 20.0 << u"20"_s;
        QTest::newRow("fraction") << 1.5 << u"1.5"_s;
        QTest::newRow("tenth") << 0.1 << u"0.1"_s;
        QTest::newRow("negative") << -3.0 << u"-3"_s;
        QTest::newRow("negative zero") << -0.0 << u"0"_s;
        QTest::newRow("sum noise") << 0.1 + 0.2 << u"0.30000000000000004"_s;
        QTest::newRow("small") << 0.000001 << u"0.000001"_s;
        QTest::newRow("tiny") << 1e-7 << u"1e-7"_s;
        QTest::newRow("large") << 123456789.0 << u"123456789"_s;
        QTest::newRow("huge") << 1e21 << u"1e+21"_s;
    }

    void printing()
    {
        QFETCH(double, value);
        QFETCH(QString, text);
        QCOMPARE(formatNumber(value), text);
    }
};

QTEST_GUILESS_MAIN(TestNumbers)
#include "tst_numbers.moc"
