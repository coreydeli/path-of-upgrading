// SPDX-License-Identifier: MIT
#include "app/itemhistory.h"
#include "core/parser.h"
#include "itemtexts.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

const Item helmetNew = item(itemtexts::helmetNew);
const Item helmetOld = item(itemtexts::helmetOld);

Item ring(int n)
{
    return item(
        u"Item Class: Rings\nRarity: Rare\nRing %1\nGold Ring\n--------\n+%1 to maximum Life"_s.arg(n));
}

QStringList names(const QList<Item> &items)
{
    QStringList names;
    std::ranges::transform(items, std::back_inserter(names), &Item::name);
    return names;
}

} // namespace

class TestItemHistory : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void newestFirst()
    {
        QTemporaryDir dir;
        ItemHistory history(dir.filePath(u"history.json"_s));
        QVERIFY(history.recent().isEmpty());
        QSignalSpy changed(&history, &ItemHistory::changed);
        history.add(helmetOld);
        history.add(helmetNew);
        QCOMPARE(names(history.recent()), QStringList({u"Doom Visage"_s, u"Grim Crown"_s}));
        QCOMPARE(changed.size(), 2);
    }

    void sameItemAgain()
    {
        QTemporaryDir dir;
        ItemHistory history(dir.filePath(u"history.json"_s));
        history.add(helmetOld);
        history.add(helmetNew);
        history.add(helmetOld);
        QCOMPARE(names(history.recent()), QStringList({u"Grim Crown"_s, u"Doom Visage"_s}));
    }

    void lately()
    {
        QTemporaryDir dir;
        ItemHistory history(dir.filePath(u"history.json"_s));
        for (int n = 1; n <= 31; ++n)
            history.add(ring(n));
        QCOMPARE(history.recent().size(), 30);
        QCOMPARE(history.recent().first().name, u"Ring 31"_s);
        QCOMPARE(history.recent().last().name, u"Ring 2"_s);
    }

    void countsStats()
    {
        QTemporaryDir dir;
        ItemHistory history(dir.filePath(u"history.json"_s));
        history.add(helmetNew);
        history.add(ring(5));
        SeenStatsLog log;
        log.note(helmetNew);
        log.note(ring(5));
        QCOMPARE(history.seenStats(), log.stats());
    }

    void savedAfterAMoment()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"history.json"_s);
        ItemHistory history(path);
        history.setSaveDelay(50ms);
        history.add(helmetNew);
        history.add(helmetOld);
        QVERIFY(!QFile::exists(path));
        QTRY_VERIFY(QFile::exists(path));

        const ItemHistory again(path);
        QCOMPARE(names(again.recent()), names(history.recent()));
        QCOMPARE(again.seenStats(), history.seenStats());
        QVERIFY(again.errorString().isEmpty());
    }

    void savedAtTheEnd()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"history.json"_s);
        {
            ItemHistory history(path);
            history.add(helmetNew);
        }
        const ItemHistory again(path);
        QCOMPARE(names(again.recent()), QStringList{u"Doom Visage"_s});
    }

    void savedOnce()
    {
        // what's saved isn't waiting any more
        QTemporaryDir dir;
        const QString path = dir.filePath(u"history.json"_s);
        {
            ItemHistory history(path);
            history.add(helmetNew);
            QVERIFY(history.save());
            QVERIFY(QFile::remove(path));
        }
        QVERIFY(!QFile::exists(path));
    }

    void nothingToSave()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"history.json"_s);
        {
            const ItemHistory history(path);
        }
        QVERIFY(!QFile::exists(path));
    }

    void forgetting()
    {
        QTemporaryDir dir;
        ItemHistory history(dir.filePath(u"history.json"_s));
        history.add(helmetNew);
        history.add(ring(5));
        const SeenStats seen = history.seenStats();
        QVERIFY(!seen.isEmpty());
        QSignalSpy changed(&history, &ItemHistory::changed);
        history.clearRecent();
        QVERIFY(history.recent().isEmpty());
        QCOMPARE(changed.size(), 1);
        // but not the stats it has seen
        QCOMPARE(history.seenStats(), seen);

        // and the next one starts the list again
        history.add(helmetOld);
        QCOMPARE(names(history.recent()), QStringList{u"Grim Crown"_s});
    }

    void forgettingNothing()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"history.json"_s);
        {
            ItemHistory history(path);
            QSignalSpy changed(&history, &ItemHistory::changed);
            history.clearRecent();
            QCOMPARE(changed.size(), 0);
        }
        // which isn't saved either
        QVERIFY(!QFile::exists(path));
    }

    void forgottenAfterAMoment()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"history.json"_s);
        ItemHistory history(path);
        history.setSaveDelay(50ms);
        history.add(helmetNew);
        QVERIFY(history.save());
        history.clearRecent();
        // the file has them until then
        QCOMPARE(names(ItemHistory(path).recent()), QStringList{u"Doom Visage"_s});
        QTRY_VERIFY(ItemHistory(path).recent().isEmpty());
        QCOMPARE(ItemHistory(path).seenStats(), history.seenStats());
    }

    void forgottenAtTheEnd()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"history.json"_s);
        {
            ItemHistory history(path);
            history.add(helmetNew);
            QVERIFY(history.save());
            history.clearRecent();
        }
        const ItemHistory again(path);
        QVERIFY(again.recent().isEmpty());
        QVERIFY(!again.seenStats().isEmpty());
    }

    void unreadable()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"history.json"_s);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ not quite");
        file.close();

        ItemHistory history(path);
        QVERIFY(history.recent().isEmpty());
        QVERIFY(!history.errorString().isEmpty());
        history.add(helmetNew);
        QVERIFY(history.save());
        QCOMPARE(names(ItemHistory(path).recent()), QStringList{u"Doom Visage"_s});
    }

    void cantSave()
    {
        // in a folder where a file is
        QTemporaryDir dir;
        QFile file(dir.filePath(u"data"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        ItemHistory history(dir.filePath(u"data/history.json"_s));
        history.add(helmetNew);
        QVERIFY(!history.save());
        QVERIFY(!history.errorString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestItemHistory)
#include "tst_itemhistory.moc"
