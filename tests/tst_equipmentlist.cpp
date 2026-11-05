// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/equipmentlist.h"
#include "app/gearlist.h"
#include "app/settingsstore.h"
#include "core/parser.h"
#include "services/pobengine.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Item item(const QString &itemClass, const QString &name, const QString &mods)
{
    return parseItem(
               u"Item Class: %1\nRarity: Rare\n%2\nIron Base\n--------\n%3"_s.arg(itemClass, name, mods))
        .value();
}

// The gear of a character you play, through the equipment list.
struct Gear
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    CharacterStore store{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    GearList list{&active, &settings};
    EquipmentList screen;
    EquipmentList others;
    EquipmentList one;
    QAbstractItemModelTester tester{&screen, QAbstractItemModelTester::FailureReportingMode::QtTest};

    Gear()
    {
        screen.setSourceModel(&list);
        others.setSourceModel(&list);
        others.setExtras(true);
        one.setSourceModel(&list);
        Character c;
        c.name = u"Tester"_s;
        c.gear.insert(u"Helmet"_s, item(u"Helmets"_s, u"Storm Crown"_s, u"+80 to maximum Life"_s));
        c.gear.insert(u"Jewel 3"_s, item(u"Jewels"_s, u"Pale Eye"_s, u"+8% to Fire Resistance"_s));
        c.gear.insert(u"Life Flask"_s, item(u"Flasks"_s, u"Dusk Flask"_s, u"Recovers Life"_s));
        active.setId(store.add(c).value_or(QString()));
    }

    static QStringList slotsOf(const QAbstractItemModel &model)
    {
        QStringList slots;
        for (int row = 0; row < model.rowCount(); ++row)
            slots << model.index(row, 0).data(GearList::SlotRole).toString();
        return slots;
    }
};

} // namespace

class TestEquipmentList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void mainSlotsInReadingOrder()
    {
        Gear g;
        // across the screen, then down it, and not the others
        QCOMPARE(Gear::slotsOf(g.screen),
                 (QStringList{u"Helmet"_s, u"Amulet"_s, u"Weapon"_s, u"Body Armour"_s, u"Offhand"_s,
                              u"Gloves"_s, u"Belt"_s, u"Ring 1"_s, u"Boots"_s, u"Ring 2"_s}));
    }

    void cells()
    {
        Gear g;
        const auto cell = [&](const QString &slot) {
            const QModelIndexList found = g.screen.match(g.screen.index(0, 0), GearList::SlotRole, slot, 1);
            return found.isEmpty() ? QPair<int, int>{-9, -9}
                                   : QPair<int, int>{found.first().data(EquipmentList::RowRole).toInt(),
                                                     found.first().data(EquipmentList::ColumnRole).toInt()};
        };
        QCOMPARE(cell(u"Helmet"_s), (QPair<int, int>{0, 1}));
        QCOMPARE(cell(u"Amulet"_s), (QPair<int, int>{0, 2}));
        QCOMPARE(cell(u"Weapon"_s), (QPair<int, int>{1, 0}));
        QCOMPARE(cell(u"Body Armour"_s), (QPair<int, int>{1, 1}));
        QCOMPARE(cell(u"Offhand"_s), (QPair<int, int>{1, 2}));
        QCOMPARE(cell(u"Boots"_s), (QPair<int, int>{3, 1}));
        QCOMPARE(g.screen.roleNames().value(EquipmentList::RowRole), QByteArray("cellRow"));
        QCOMPARE(g.screen.roleNames().value(EquipmentList::ColumnRole), QByteArray("cellColumn"));
        // and it still has what the gear list does
        QCOMPARE(g.screen.roleNames().value(GearList::NameRole), QByteArray("name"));
        const QModelIndex helmet = g.screen.index(0, 0);
        QCOMPARE(helmet.data(GearList::NameRole).toString(), u"Storm Crown"_s);
    }

    void others()
    {
        Gear g;
        QCOMPARE(g.others.rowCount(), 2);
        const QStringList slots = Gear::slotsOf(g.others);
        QVERIFY(slots.contains(u"Jewel 3"_s));
        QVERIFY(slots.contains(u"Life Flask"_s));
        // they have no place on the screen
        QCOMPARE(g.others.index(0, 0).data(EquipmentList::RowRole).toInt(), -1);
        QCOMPARE(g.others.index(0, 0).data(EquipmentList::ColumnRole).toInt(), -1);
    }

    void followsTheGear()
    {
        Gear g;
        QVERIFY(g.active.wear(u"Charm 1"_s, item(u"Charms"_s, u"Ash Charm"_s, u"Recovers Life"_s)));
        QCOMPARE(g.others.rowCount(), 3);
        QVERIFY(Gear::slotsOf(g.others).contains(u"Charm 1"_s));
        QCOMPARE(g.screen.rowCount(), 10);
    }

    void onlyOneSlot()
    {
        Gear g;
        QSignalSpy changed(&g.one, &EquipmentList::onlyChanged);
        QVERIFY(g.one.rowCount() > 0);
        g.one.setOnly(u"Helmet"_s);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(Gear::slotsOf(g.one), QStringList{u"Helmet"_s});
        // whichever kind it is
        g.one.setOnly(u"Jewel 3"_s);
        QCOMPARE(Gear::slotsOf(g.one), QStringList{u"Jewel 3"_s});
        g.one.setOnly(u"Jewel 3"_s);
        QCOMPARE(changed.size(), 2);
        // and nothing for one that isn't there
        g.one.setOnly(u"Tail"_s);
        QCOMPARE(g.one.rowCount(), 0);
        // an empty one is the main slots again
        g.one.setOnly(QString());
        QCOMPARE(g.one.rowCount(), 10);
    }

    void extrasSignal()
    {
        Gear g;
        QSignalSpy changed(&g.others, &EquipmentList::extrasChanged);
        g.others.setExtras(true);
        QCOMPARE(changed.size(), 0);
        g.others.setExtras(false);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(g.others.rowCount(), 10);
    }

    void noSource()
    {
        EquipmentList list;
        QCOMPARE(list.rowCount(), 0);
        list.setOnly(u"Helmet"_s);
        QCOMPARE(list.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(TestEquipmentList)
#include "tst_equipmentlist.moc"
