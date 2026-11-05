// SPDX-License-Identifier: MIT
#include "app/checkeditems.h"
#include "app/itemhistory.h"
#include "core/parser.h"
#include "itemtexts.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

const Item helmetNew = item(itemtexts::helmetNew);
const Item helmetOld = item(itemtexts::helmetOld);
const Item magicRing =
    item(u"Item Class: Rings\nRarity: Magic\nHale Gold Ring of the Whelpling\n--------\nItem Level: 20\n"
         "--------\n+30 to maximum Life"_s);
const Item belt = item(u"Item Class: Belts\nRarity: Normal\nLeather Belt\n--------\nItem Level: 12"_s);

// The items you checked, as the window lists them.
struct Checked
{
    QTemporaryDir dir;
    ItemHistory history{dir.filePath(u"history.json"_s)};
    CheckedItems checked{&history};
    QAbstractItemModelTester tester{&checked, QAbstractItemModelTester::FailureReportingMode::QtTest};

    QVariant at(int row, CheckedItems::Role role) const { return checked.index(row).data(role); }

    QStringList names() const
    {
        QStringList names;
        for (int row = 0; row < checked.rowCount(); ++row)
            names << at(row, CheckedItems::NameRole).toString();
        return names;
    }
};

} // namespace

class TestCheckedItems : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void none()
    {
        Checked c;
        QCOMPARE(c.checked.rowCount(), 0);
        c.checked.checkAgain(0);
        c.checked.forget();
        QCOMPARE(c.checked.rowCount(), 0);
    }

    void limit()
    {
        // what the page says, and how many the history keeps
        Checked c;
        QCOMPARE(c.checked.limit(), 30);
        QCOMPARE(c.checked.property("limit").toInt(), 30);
        for (int i = 0; i < 40; ++i)
            c.history.add(
                item(u"Item Class: Rings\nRarity: Rare\nRing "_s + QString::number(i) +
                     u"\nSapphire Ring\n--------\nItem Level: 75\n--------\n+10 to maximum Life"_s));
        QCOMPARE(c.checked.rowCount(), c.checked.limit());
    }

    void newestFirst()
    {
        Checked c;
        c.history.add(helmetOld);
        c.history.add(helmetNew);
        QCOMPARE(c.names(), QStringList({u"Doom Visage"_s, u"Grim Crown"_s}));
        // checked again, it moves up
        c.history.add(helmetOld);
        QCOMPARE(c.names(), QStringList({u"Grim Crown"_s, u"Doom Visage"_s}));
    }

    void rowsChange()
    {
        Checked c;
        QSignalSpy reset(&c.checked, &QAbstractItemModel::modelReset);
        c.history.add(helmetOld);
        QCOMPARE(reset.size(), 1);
        c.history.add(helmetNew);
        QCOMPARE(reset.size(), 2);
        c.history.clearRecent();
        QCOMPARE(reset.size(), 3);
    }

    void lately()
    {
        Checked c;
        for (int n = 1; n <= 31; ++n)
            c.history.add(item(u"Item Class: Rings\nRarity: Rare\nRing %1\nGold Ring\n--------\n+%1 to "
                               "maximum Life"_s.arg(n)));
        QCOMPARE(c.checked.rowCount(), 30);
        QCOMPARE(c.names().first(), u"Ring 31"_s);
        QCOMPARE(c.names().last(), u"Ring 2"_s);
    }

    void whatItIs()
    {
        Checked c;
        c.history.add(helmetNew);
        QCOMPARE(c.at(0, CheckedItems::NameRole), u"Doom Visage"_s);
        QCOMPARE(c.checked.index(0).data(Qt::DisplayRole), u"Doom Visage"_s);
        QCOMPARE(c.at(0, CheckedItems::BaseRole), u"Expert Visored Helm"_s);
        QCOMPARE(c.at(0, CheckedItems::ClassRole), u"Helmets"_s);
        QCOMPARE(c.at(0, CheckedItems::RarityRole), u"Rare"_s);
        QCOMPARE(c.at(0, CheckedItems::ItemLevelRole), 79);
        QCOMPARE(c.at(0, CheckedItems::RequiredLevelRole), 65);
        QVERIFY(!c.checked.index(0).data(Qt::UserRole).isValid());
        // and nothing under it
        QCOMPARE(c.checked.rowCount(c.checked.index(0)), 0);
    }

    void roleNames()
    {
        // which the window's rows are made of
        Checked c;
        const QHash<int, QByteArray> names = c.checked.roleNames();
        QCOMPARE(names.size(), 6);
        QCOMPARE(names.value(CheckedItems::NameRole), QByteArray("name"));
        QCOMPARE(names.value(CheckedItems::BaseRole), QByteArray("base"));
        QCOMPARE(names.value(CheckedItems::ClassRole), QByteArray("itemClass"));
        QCOMPARE(names.value(CheckedItems::RarityRole), QByteArray("rarity"));
        QCOMPARE(names.value(CheckedItems::ItemLevelRole), QByteArray("itemLevel"));
        QCOMPARE(names.value(CheckedItems::RequiredLevelRole), QByteArray("requiredLevel"));
    }

    void withoutLevels()
    {
        Checked c;
        c.history.add(helmetOld);
        QCOMPARE(c.at(0, CheckedItems::ItemLevelRole), 50);
        QCOMPARE(c.at(0, CheckedItems::RequiredLevelRole), 0);
    }

    void noBaseType()
    {
        // a magic item has its affixes in its name and no base type to say
        Checked c;
        c.history.add(magicRing);
        QCOMPARE(c.at(0, CheckedItems::NameRole), u"Hale Gold Ring of the Whelpling"_s);
        QCOMPARE(c.at(0, CheckedItems::BaseRole), QString());
        QCOMPARE(c.at(0, CheckedItems::RarityRole), u"Magic"_s);
    }

    void baseTypeIsTheName()
    {
        // as it is for normal items
        Checked c;
        c.history.add(belt);
        QCOMPARE(c.at(0, CheckedItems::NameRole), u"Leather Belt"_s);
        QCOMPARE(c.at(0, CheckedItems::BaseRole), QString());
    }

    void noName()
    {
        Checked c;
        Item unnamed = helmetNew;
        unnamed.name.clear();
        c.history.add(unnamed);
        QCOMPARE(c.at(0, CheckedItems::NameRole), u"Expert Visored Helm"_s);
        QCOMPARE(c.at(0, CheckedItems::BaseRole), QString());
    }

    // the lines as (text, tag) pairs
    static QList<QPair<QString, QString>> lines(const QVariant &list)
    {
        QList<QPair<QString, QString>> found;
        for (const QVariant &line : list.toList())
            found << qMakePair(line.toMap().value(u"text"_s).toString(),
                               line.toMap().value(u"tag"_s).toString());
        return found;
    }

    void details()
    {
        Checked c;
        c.history.add(helmetOld);
        c.history.add(helmetNew);
        const QVariantMap newest = c.checked.details(0);
        QCOMPARE(newest.value(u"name"_s), u"Doom Visage"_s);
        QCOMPARE(newest.value(u"base"_s), u"Expert Visored Helm"_s);
        QCOMPARE(newest.value(u"itemClass"_s), u"Helmets"_s);
        QCOMPARE(newest.value(u"rarity"_s), u"Rare"_s);
        QCOMPARE(newest.value(u"itemLevel"_s), 79);
        QCOMPARE(newest.value(u"requiredLevel"_s), 65);
        QCOMPARE(newest.value(u"corrupted"_s), false);

        // the base properties apart from the mods, each with what kind it is
        const auto properties = lines(newest.value(u"properties"_s));
        QCOMPARE(properties.size(), 1);
        QVERIFY(properties.first().first.startsWith(u"Armour: 312"_s));
        QCOMPARE(properties.first().second, u"base"_s);
        const auto mods = lines(newest.value(u"mods"_s));
        QCOMPARE(mods.size(), 6);
        QVERIFY(mods.at(0).first.contains(u"Cold Resistance"_s));
        QCOMPARE(mods.at(0).second, u"rune"_s);
        QVERIFY(mods.at(1).first.contains(u"maximum Life"_s));
        QCOMPARE(mods.at(1).second, u"explicit"_s);

        // and the other row's own
        const QVariantMap older = c.checked.details(1);
        QCOMPARE(older.value(u"name"_s), u"Grim Crown"_s);
        QCOMPARE(older.value(u"requiredLevel"_s), 0);
        QCOMPARE(lines(older.value(u"mods"_s)).size(), 3);
    }

    void detailsOfACorruptedItem()
    {
        Checked c;
        Item corrupted = belt;
        corrupted.corrupted = true;
        c.history.add(corrupted);
        QCOMPARE(c.checked.details(0).value(u"corrupted"_s), true);
        QVERIFY(lines(c.checked.details(0).value(u"mods"_s)).isEmpty());
    }

    void detailsOfNothing()
    {
        Checked c;
        QVERIFY(c.checked.details(0).isEmpty());
        c.history.add(belt);
        QVERIFY(c.checked.details(1).isEmpty());
        QVERIFY(c.checked.details(-1).isEmpty());
        QVERIFY(!c.checked.details(0).isEmpty());
    }

    void checkingAgain()
    {
        Checked c;
        c.history.add(helmetOld);
        c.history.add(helmetNew);
        QSignalSpy asked(&c.checked, &CheckedItems::checkAgainAsked);
        c.checked.checkAgain(1);
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.first().at(0).value<Item>().name, u"Grim Crown"_s);
        QCOMPARE(asked.first().at(0).value<Item>().raw, helmetOld.raw);
        // which doesn't change the list itself
        QCOMPARE(c.names(), QStringList({u"Doom Visage"_s, u"Grim Crown"_s}));
    }

    void checkingAgainMovesIt()
    {
        // as the app does, which has the history take it as one more checked
        Checked c;
        c.history.add(helmetOld);
        c.history.add(helmetNew);
        QList<Item> got;
        connect(&c.checked, &CheckedItems::checkAgainAsked, &c.checked, [&](const Item &again) {
            c.history.add(again);
            got << again;
        });
        c.checked.checkAgain(1);
        QCOMPARE(got.size(), 1);
        QCOMPARE(got.first().raw, helmetOld.raw);
        QCOMPARE(c.names(), QStringList({u"Grim Crown"_s, u"Doom Visage"_s}));
    }

    void rowsThatAreNotThere()
    {
        Checked c;
        c.history.add(helmetOld);
        QSignalSpy asked(&c.checked, &CheckedItems::checkAgainAsked);
        c.checked.checkAgain(1);
        c.checked.checkAgain(-1);
        QCOMPARE(asked.size(), 0);
        c.checked.checkAgain(0);
        QCOMPARE(asked.size(), 1);
    }

    void forgetting()
    {
        Checked c;
        c.history.add(helmetOld);
        c.history.add(helmetNew);
        const SeenStats seen = c.history.seenStats();
        c.checked.forget();
        QCOMPARE(c.checked.rowCount(), 0);
        QVERIFY(c.history.recent().isEmpty());
        // but the stat picker keeps the stats
        QCOMPARE(c.history.seenStats(), seen);
        QVERIFY(!seen.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCheckedItems)
#include "tst_checkeditems.moc"
