// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/seenstats.h"
#include "itemtexts.h"

#include <QTest>

using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

const QString life = u"# to maximum Life"_s;

Item ringWithLife(int amount)
{
    Item ring;
    ring.itemClass = u"Rings"_s;
    ring.stats = StatMap({{life, double(amount)}});
    return ring;
}

} // namespace

class TestSeenStats : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void countsByClass()
    {
        SeenStatsLog log;
        QVERIFY(log.note(item(itemtexts::helmetNew)));
        QVERIFY(log.note(item(itemtexts::helmetOld)));
        QVERIFY(log.note(item(itemtexts::advancedRing)));

        const QHash<QString, int> helmets = log.stats().value(u"Helmets"_s);
        QCOMPARE(helmets.value(life), 2);
        QCOMPARE(helmets.value(u"# to Strength"_s), 1);
        QCOMPARE(log.stats().value(u"Rings"_s).value(u"# to Strength"_s), 1);
        QCOMPARE(log.stats().size(), 2);
    }

    void sameItemOnce()
    {
        SeenStatsLog log;
        QVERIFY(log.note(item(itemtexts::advancedRing)));
        QVERIFY(!log.note(item(itemtexts::advancedRing)));

        // and copied without Ctrl+Alt+C
        const QString plain = u"Item Class: Rings\nRarity: Rare\nStorm Loop\nSapphire Ring\n--------\n"
                              "Requirements:\nLevel: 40\n--------\nItem Level: 75\n--------\n"
                              "+24% to Cold Resistance (implicit)\n--------\n+62 to maximum Life\n"
                              "+30% to Lightning Resistance\n+12 to Strength and Dexterity"_s;
        QVERIFY(!log.note(item(plain)));
        QCOMPARE(log.stats().value(u"Rings"_s).value(life), 1);
    }

    void nothingToCount()
    {
        SeenStatsLog log;
        QVERIFY(!log.note(Item()));
        Item bare = ringWithLife(40);
        bare.itemClass.clear();
        QVERIFY(!log.note(bare));
        QVERIFY(log.stats().isEmpty());
    }

    void forgetsTheOldest()
    {
        SeenStatsLog log;
        for (int n = 1; n <= 2001; ++n)
            QVERIFY(log.note(ringWithLife(n)));
        QVERIFY(!log.note(ringWithLife(2001)));
        QVERIFY(!log.note(ringWithLife(2)));
        QVERIFY(log.note(ringWithLife(1)));
        QCOMPARE(log.stats().value(u"Rings"_s).value(life), 2002);
    }

    void jsonRoundTrip()
    {
        SeenStatsLog log;
        log.note(item(itemtexts::helmetNew));
        log.note(item(itemtexts::advancedRing));

        SeenStatsLog back = SeenStatsLog::fromJson(log.toJson());
        QCOMPARE(back.stats(), log.stats());
        QVERIFY(!back.note(item(itemtexts::helmetNew)));
        QVERIFY(back.note(item(itemtexts::helmetOld)));
    }

    void savedCountsMustBeCounts()
    {
        const QJsonObject saved = {
            {u"byClass"_s, QJsonObject{{u"Rings"_s, QJsonObject{{life, 2},
                                                                {u"# to Strength"_s, 0},
                                                                {u"# to Dexterity"_s, -3},
                                                                {u"# to Intelligence"_s, u"many"_s}}}}},
        };
        const SeenStatsLog log = SeenStatsLog::fromJson(saved);
        QCOMPARE(log.stats().value(u"Rings"_s), (QHash<QString, int>{{life, 2}}));
    }
};

QTEST_GUILESS_MAIN(TestSeenStats)
#include "tst_seenstats.moc"
