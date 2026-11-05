// SPDX-License-Identifier: MIT
#include "core/slots.h"

#include <QSet>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestSlots : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void classToSlot_data()
    {
        QTest::addColumn<QString>("itemClass");
        QTest::addColumn<QString>("slot");
        QTest::newRow("helmet") << u"Helmets"_s << u"Helmet"_s;
        QTest::newRow("body") << u"Body Armours"_s << u"Body Armour"_s;
        QTest::newRow("ring") << u"Rings"_s << u"Ring"_s;
        QTest::newRow("focus") << u"Foci"_s << u"Offhand"_s;
        QTest::newRow("quiver") << u"Quivers"_s << u"Offhand"_s;
        QTest::newRow("jewel") << u"Jewels"_s << u"Jewel"_s;
        QTest::newRow("time-lost jewel") << u"Time-Lost Jewels"_s << u"Jewel"_s;
        QTest::newRow("life flask") << u"Life Flasks"_s << u"Life Flask"_s;
        QTest::newRow("mana flask") << u"Mana Flasks"_s << u"Mana Flask"_s;
        QTest::newRow("two hand mace") << u"Two Hand Maces"_s << u"Weapon"_s;
        QTest::newRow("quarterstaff") << u"Quarterstaves"_s << u"Weapon"_s;
        QTest::newRow("crossbow") << u"Crossbows"_s << u"Weapon"_s;
        QTest::newRow("padded") << u"  helmets "_s << u"Helmet"_s;
        QTest::newRow("currency") << u"Stackable Currency"_s << QString();
        QTest::newRow("gem") << u"Skill Gems"_s << QString();
        QTest::newRow("empty") << QString() << QString();
    }

    void classToSlot()
    {
        QFETCH(QString, itemClass);
        QFETCH(QString, slot);
        QCOMPARE(slotForClass(itemClass), slot);
    }

    void twoHanded()
    {
        QVERIFY(isTwoHanded(u"Two Hand Maces"_s));
        QVERIFY(isTwoHanded(u"Quarterstaves"_s));
        QVERIFY(isTwoHanded(u"Crossbows"_s));
        QVERIFY(!isTwoHanded(u"One Hand Maces"_s));
        QVERIFY(!isTwoHanded(u"Bows"_s));
        QVERIFY(!isTwoHanded(QString()));
    }

    void groups()
    {
        QCOMPARE(groupOf(u"Ring 2"_s), u"Ring"_s);
        QCOMPARE(groupOf(u"Weapon 2"_s), u"Weapon"_s);
        QCOMPARE(groupOf(u"Jewel 4"_s), u"Jewel"_s);
        QCOMPARE(groupOf(u"Helmet"_s), u"Helmet"_s);
        QCOMPARE(slotGroup(u"Ring"_s), QStringList({u"Ring 1"_s, u"Ring 2"_s}));
        QCOMPARE(slotGroup(u"Helmet"_s), QStringList({u"Helmet"_s}));
        QVERIFY(comparesWithWeakest(u"Jewel"_s));
        QVERIFY(comparesWithWeakest(u"Ring"_s));
        QVERIFY(comparesWithWeakest(u"Charm"_s));
        QVERIFY(!comparesWithWeakest(u"Life Flask"_s));
        QVERIFY(!comparesWithWeakest(u"Weapon"_s));
        QCOMPARE(allSlots().size(), mainSlots().size() + extraSlots().size());
        QCOMPARE(mainSlots().first(), u"Weapon"_s);
    }

    void equipmentScreen()
    {
        // every main slot has a cell of its own, and no other slot does
        QSet<std::pair<int, int>> taken;
        for (const QString &slot : mainSlots()) {
            const auto cell = equipmentCell(slot);
            QVERIFY2(cell, qPrintable(slot));
            QVERIFY(cell->column >= 0 && cell->column < 3);
            QVERIFY(!taken.contains({cell->row, cell->column}));
            taken.insert({cell->row, cell->column});
        }
        QCOMPARE(taken.size(), mainSlots().size());
        for (const QString &slot : extraSlots())
            QVERIFY(!equipmentCell(slot));
        QVERIFY(!equipmentCell(QString()));

        // the helmet over the body with the amulet beside it, the weapons either side
        QCOMPARE(equipmentCell(u"Helmet"_s)->column, equipmentCell(u"Body Armour"_s)->column);
        QCOMPARE(equipmentCell(u"Helmet"_s)->row, 0);
        QCOMPARE(equipmentCell(u"Amulet"_s)->row, 0);
        QCOMPARE(equipmentCell(u"Weapon"_s)->row, equipmentCell(u"Body Armour"_s)->row);
        QCOMPARE(equipmentCell(u"Offhand"_s)->row, equipmentCell(u"Body Armour"_s)->row);
        QVERIFY(equipmentCell(u"Weapon"_s)->column < equipmentCell(u"Body Armour"_s)->column);
        QVERIFY(equipmentCell(u"Offhand"_s)->column > equipmentCell(u"Body Armour"_s)->column);
        // the belt under the body, the boots at the bottom
        QCOMPARE(equipmentCell(u"Belt"_s)->column, equipmentCell(u"Body Armour"_s)->column);
        QVERIFY(equipmentCell(u"Belt"_s)->row > equipmentCell(u"Body Armour"_s)->row);
        for (const QString &slot : mainSlots())
            QVERIFY(equipmentCell(u"Boots"_s)->row >= equipmentCell(slot)->row);
        // the gloves and rings below the weapons
        for (const QString &slot : {u"Gloves"_s, u"Ring 1"_s, u"Ring 2"_s})
            QVERIFY(equipmentCell(slot)->row > equipmentCell(u"Weapon"_s)->row);
    }

    void hands()
    {
        QCOMPARE(otherHand(u"Weapon"_s), u"Offhand"_s);
        QCOMPARE(otherHand(u"Weapon 2"_s), u"Offhand 2"_s);
        QCOMPARE(otherHand(u"Offhand 2"_s), u"Weapon 2"_s);
        QCOMPARE(otherHand(u"Ring 1"_s), QString());
    }
};

QTEST_GUILESS_MAIN(TestSlots)
#include "tst_slots.moc"
