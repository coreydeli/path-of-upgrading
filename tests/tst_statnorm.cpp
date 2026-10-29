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

    void conditionalMods()
    {
        QVERIFY(pou::isConditionalStat(u"Minions deal #% increased Damage if you've Hit Recently"_s));
        QVERIFY(pou::isConditionalStat(u"#% increased Damage while on Low Life"_s));
        QVERIFY(pou::isConditionalStat(u"#% increased Attack Speed per Frenzy Charge"_s));
        QVERIFY(!pou::isConditionalStat(u"# Life Regeneration per second"_s));
        QVERIFY(!pou::isConditionalStat(u"# to maximum Life"_s));
        // whole words only
        QVERIFY(!pou::isConditionalStat(u"#% increased Damage with Whips"_s));
    }

    void names()
    {
        QCOMPARE(pou::statName(u"[Base] Armour"_s), u"Armour"_s);
        QCOMPARE(pou::statName(u"[Weapon] Total DPS"_s), u"Total DPS"_s);
        QCOMPARE(pou::statName(u"# to maximum Life"_s), u"# to maximum Life"_s);
        // a mod that only starts with a bracket
        QCOMPARE(pou::statName(u"[Totem] #% increased Life"_s), u"[Totem] #% increased Life"_s);
        QVERIFY(pou::isItemProperty(u"[Base] Spirit"));
        QVERIFY(!pou::isItemProperty(u"#% increased Spirit"));
    }
};

QTEST_GUILESS_MAIN(TestStatNorm)
#include "tst_statnorm.moc"
