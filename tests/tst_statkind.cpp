// SPDX-License-Identifier: MIT
#include "app/statkinds.h"
#include "core/statkind.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

Q_DECLARE_METATYPE(pou::StatKind)

class TestStatKind : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void kinds_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<StatKind>("kind");

        QTest::newRow("fire") << u"Fire Resistance 30% or more"_s << StatKind::Resistance;
        QTest::newRow("all") << u"+#% to all Elemental Resistances"_s << StatKind::Resistance;
        QTest::newRow("chaos") << u"chaos resistance"_s << StatKind::Resistance;
        QTest::newRow("penetration") << u"Damage Penetrates #% Fire Resistance"_s << StatKind::Damage;

        QTest::newRow("life") << u"Maximum Life 60 or more"_s << StatKind::Defence;
        QTest::newRow("shield") << u"#% increased Energy Shield"_s << StatKind::Defence;
        QTest::newRow("armour") << u"+# to Armour"_s << StatKind::Defence;
        QTest::newRow("evasion") << u"Evasion Rating"_s << StatKind::Defence;
        QTest::newRow("block") << u"Chance to Block Attack Damage"_s << StatKind::Defence;
        QTest::newRow("taken") << u"#% reduced Damage taken"_s << StatKind::Defence;
        QTest::newRow("stun") << u"Stun Threshold"_s << StatKind::Defence;

        QTest::newRow("damage") << u"#% increased Physical Damage"_s << StatKind::Damage;
        QTest::newRow("speed") << u"#% increased Attack Speed"_s << StatKind::Damage;
        QTest::newRow("crit") << u"#% increased Critical Hit Chance"_s << StatKind::Damage;
        QTest::newRow("cast") << u"#% increased Cast Speed"_s << StatKind::Damage;
        QTest::newRow("skills") << u"+# to Level of all Spell Skills"_s << StatKind::Damage;

        QTest::newRow("mana") << u"Maximum Mana"_s << StatKind::Other;
        QTest::newRow("movement") << u"#% increased Movement Speed"_s << StatKind::Other;
        QTest::newRow("rarity") << u"#% increased Rarity of Items found"_s << StatKind::Other;
        QTest::newRow("empty") << QString() << StatKind::Other;
        // whole words, so these don't count
        QTest::newRow("lifetime") << u"Lifetime"_s << StatKind::Other;
        QTest::newRow("casting") << u"Forecast"_s << StatKind::Other;
    }

    void kinds()
    {
        QFETCH(QString, text);
        QFETCH(StatKind, kind);
        QCOMPARE(statKindOf(text), kind);
    }

    void theCaseDoesntMatter() { QCOMPARE(statKindOf(u"FIRE RESISTANCE"_s), StatKind::Resistance); }

    // QML sees the same values
    void qmlNamesTheSameKinds()
    {
        QCOMPARE(int(ChipKind::Other), int(StatKind::Other));
        QCOMPARE(int(ChipKind::Resistance), int(StatKind::Resistance));
        QCOMPARE(int(ChipKind::Damage), int(StatKind::Damage));
        QCOMPARE(int(ChipKind::Defence), int(StatKind::Defence));
        QCOMPARE(StatKinds::of(u"Fire Resistance"_s), ChipKind::Resistance);
        QCOMPARE(StatKinds::of(u"Maximum Life"_s), ChipKind::Defence);
    }
};

QTEST_GUILESS_MAIN(TestStatKind)
#include "tst_statkind.moc"
