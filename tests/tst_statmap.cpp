// SPDX-License-Identifier: MIT
#include "core/statmap.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using pou::StatMap;

class TestStatMap : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsInsertionOrder()
    {
        StatMap m;
        m.add(u"b"_s, 1);
        m.add(u"a"_s, 2);
        m.add(u"b"_s, 3);
        m.set(u"c"_s, 4);
        m.set(u"a"_s, 5);
        QCOMPARE(m.keys(), QStringList({u"b"_s, u"a"_s, u"c"_s}));
        QCOMPARE(m.value(u"b"), 4.0);
        QCOMPARE(m.value(u"a"), 5.0);
    }

    void missingKeys()
    {
        StatMap m{{u"life"_s, 10}};
        QVERIFY(!m.get(u"mana").has_value());
        QCOMPARE(m.value(u"mana"), 0.0);
        QCOMPARE(m.value(u"mana", -1), -1.0);
        QVERIFY(m.contains(u"life"));
        QVERIFY(m.remove(u"life"));
        QVERIFY(!m.remove(u"life"));
        QVERIFY(m.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestStatMap)
#include "tst_statmap.moc"
