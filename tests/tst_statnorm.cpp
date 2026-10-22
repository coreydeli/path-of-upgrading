// SPDX-License-Identifier: MIT
#include "core/statnorm.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;

class TestStatNorm : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sameStatDifferentWording()
    {
        const QString key = pou::normalizeStat(u"# to maximum Life"_s);
        QCOMPARE(key, u"# maximum life"_s);
        QCOMPARE(pou::normalizeStat(u"+# to maximum Life"_s), key);
        QCOMPARE(pou::normalizeStat(u"+# total maximum Life"_s), key);
    }

    void localSuffixIsDropped()
    {
        QCOMPARE(pou::normalizeStat(u"#% increased Armour (Local)"_s), u"#% increased armour"_s);
        QCOMPARE(pou::normalizeStat(u"#% increased Armour"_s), u"#% increased armour"_s);
    }

    void onlyWholeWordsAreFiller()
    {
        // "Totem" and "Stop" must survive
        QCOMPARE(pou::normalizeStat(u"#% increased Totem Life"_s), u"#% increased totem life"_s);
        QCOMPARE(pou::normalizeStat(u"#% to Stop"_s), u"#% stop"_s);
    }
};

QTEST_GUILESS_MAIN(TestStatNorm)
#include "tst_statnorm.moc"
