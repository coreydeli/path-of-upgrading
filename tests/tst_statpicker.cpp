// SPDX-License-Identifier: MIT
#include "app/itemhistory.h"
#include "app/statpicker.h"
#include "core/datafiles.h"
#include "core/parser.h"
#include "fakenetwork.h"
#include "services/tradeclient.h"
#include "services/tradedatastore.h"
#include "tradefixtures.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString life = u"# to maximum Life"_s;
const QString fire = u"#% to Fire Resistance"_s;
const QString armour = u"#% increased Armour"_s;
const QString speed = u"#% increased Movement Speed"_s;
const QString rarity = u"#% increased Rarity of Items found"_s;

const QStringList properties = {
    u"[Base] Armour"_s,          u"[Weapon] Attacks per Second"_s,
    u"[Base] Block Chance"_s,    u"[Weapon] Critical Hit Chance"_s,
    u"[Weapon] Elemental DPS"_s, u"[Base] Energy Shield"_s,
    u"[Base] Evasion Rating"_s,  u"[Weapon] Physical DPS"_s,
    u"[Base] Runic Ward"_s,      u"[Base] Spirit"_s,
    u"[Weapon] Total DPS"_s,
};

// The fixture's stats, fire resistance on runes, and one that only comes as
// a total.
QJsonObject stats()
{
    using namespace tradefixtures;
    QJsonObject answer = tradefixtures::stats();
    QJsonArray groups = answer[u"result"].toArray();
    groups << group(u"rune"_s, {stat(u"rune.stat_fire"_s, u"+#% to Fire Resistance"_s, u"rune"_s),
                                stat(u"augment.stat_fire"_s, u"+#% to Fire Resistance"_s, u"augment"_s)});
    groups << group(u"pseudo_more"_s, {stat(u"pseudo.pseudo_total_elemental_resistance"_s,
                                            u"+#% total Elemental Resistance"_s, u"pseudo"_s)});
    answer[u"result"] = groups;
    return answer;
}

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

Item item(const QString &text)
{
    return parseItem(text).value();
}

const Item ring = item(u"Item Class: Rings\nRarity: Rare\nDoom Loop\nGold Ring\n--------\n"
                       u"+30 to maximum Life\n12% increased Rarity of Items found"_s);
const Item mace = item(u"Item Class: Two Hand Maces\nRarity: Rare\nGrim Thud\nTemple Maul\n--------\n"
                       u"Physical Damage: 30-50\nChaos Damage: 10-20\nAttacks per Second: 1.20"_s);
const Item helmet = item(u"Item Class: Helmets\nRarity: Rare\nGrim Peak\nFeathered Hood\n--------\n"
                         u"Evasion Rating: 90\n--------\n+40 to maximum Life"_s);

struct Picking
{
    QTemporaryDir dir;
    FakeNetwork network;
    TradeClient client{&network};
    TradeDataStore data{&client, dir.path()};
    ItemHistory history{dir.filePath(u"history.json"_s)};
    StatPicker picker{&data, &history};

    // What the site says when it's asked.
    Picking()
    {
        const QJsonObject leagues{{u"result"_s, QJsonArray{QJsonObject{{u"id"_s, u"Standard"_s}}}}};
        network.answers = {
            {.body = json(stats())}, {.body = json(tradefixtures::filters())}, {.body = json(leagues)}};
    }

    QStringList keys() const
    {
        QStringList keys;
        for (int row = 0; row < picker.rowCount(); ++row)
            keys << picker.index(row).data(StatPicker::KeyRole).toString();
        return keys;
    }

    QVariant of(const QString &key, StatPicker::Role role) const
    {
        const qsizetype row = keys().indexOf(key);
        return row < 0 ? QVariant() : picker.index(int(row)).data(role);
    }

    // Asks the site, and waits for what it says.
    bool refreshFromSite()
    {
        picker.setSiteAllowed(true);
        QSignalSpy reset(&picker, &StatPicker::modelReset);
        picker.refresh();
        return reset.wait();
    }
};

} // namespace

class TestStatPicker : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what it says when the site lets it down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.trade.warning=false"_s);
    }

    void nothingYet()
    {
        Picking p;
        QCOMPARE(p.picker.rowCount(), 0);
        QSignalSpy messageChanged(&p.picker, &StatPicker::messageChanged);
        p.picker.refresh();
        QCOMPARE(p.keys(), properties);
        QCOMPARE(p.of(u"[Base] Armour"_s, StatPicker::NameRole).toString(), u"Armour"_s);
        QCOMPARE(p.of(u"[Base] Armour"_s, StatPicker::NoteRole).toString(),
                 u"The item's own, with its mods"_s);
        QCOMPARE(p.of(u"[Weapon] Total DPS"_s, StatPicker::NoteRole).toString(),
                 u"The weapon's own, with its mods"_s);
        QCOMPARE(p.picker.message(),
                 u"The trade site's list of stats comes with trade search, so here are only "
                 u"the item's own and those on the items you've checked."_s);
        QVERIFY(p.network.sent.isEmpty());

        // said once, as it's the same the next time
        QCOMPARE(messageChanged.size(), 1);
        p.picker.refresh();
        QCOMPARE(messageChanged.size(), 1);
    }

    void siteStats()
    {
        Picking p;
        QSignalSpy messageChanged(&p.picker, &StatPicker::messageChanged);
        QVERIFY(p.refreshFromSite());
        QCOMPARE(messageChanged.size(), 2);
        QCOMPARE(p.picker.message(), QString());
        // and not the total elemental resistance
        QCOMPARE(p.keys(), properties + QStringList({life, armour, speed, fire}));
        QCOMPARE(p.of(life, StatPicker::NameRole).toString(), life);
        QCOMPARE(p.of(life, StatPicker::NoteRole).toString(), u"Explicit"_s);
        QCOMPARE(p.of(fire, StatPicker::NoteRole).toString(), u"Explicit, Rune"_s);
    }

    void gettingThem()
    {
        Picking p;
        p.picker.setSiteAllowed(true);
        p.picker.refresh();
        QCOMPARE(p.picker.message(), u"Getting the list of stats from the trade site."_s);
        QCOMPARE(p.keys(), properties);
    }

    void cantGetThem()
    {
        Picking p;
        p.network.answers = {{.error = QNetworkReply::HostNotFoundError}, {}, {}};
        QVERIFY(p.refreshFromSite());
        QCOMPARE(p.keys(), properties);
        QCOMPARE(p.picker.message(),
                 u"Couldn't get the trade site's list of stats, so here are only the item's "
                 u"own and those on the items you've checked."_s);
    }

    void keptStats()
    {
        Picking p;
        QVERIFY(writeJsonFile(p.dir.filePath(u"stats.json"_s), stats()));
        p.picker.refresh();
        QVERIFY(p.network.sent.isEmpty());
        QCOMPARE(p.keys(), properties + QStringList({life, armour, speed, fire}));
        QCOMPARE(p.picker.message(), QString());
    }

    void onlyWhenOpened()
    {
        Picking p;
        QVERIFY(p.refreshFromSite());
        QSignalSpy reset(&p.picker, &StatPicker::modelReset);
        QSignalSpy refreshed(&p.data, &TradeDataStore::refreshed);
        p.data.refresh();
        QVERIFY(refreshed.wait());
        QCOMPARE(reset.size(), 0);
    }

    void seenStats()
    {
        Picking p;
        p.history.add(ring);
        p.history.add(helmet);
        QVERIFY(p.refreshFromSite());
        // the ones on the most items first
        QStringList seenFirst = properties;
        seenFirst.move(seenFirst.indexOf(u"[Base] Evasion Rating"_s), 0);
        QCOMPARE(p.keys(), seenFirst + QStringList({life, rarity, armour, speed, fire}));
        QCOMPARE(p.of(life, StatPicker::NoteRole).toString(),
                 u"Explicit. On 2 of the items you've checked"_s);
        QCOMPARE(p.of(rarity, StatPicker::NoteRole).toString(),
                 u"Not on the trade site. On one of the items you've checked"_s);
        QCOMPARE(p.of(u"[Base] Evasion Rating"_s, StatPicker::NoteRole).toString(),
                 u"The item's own, with its mods. On one of the items you've checked"_s);
    }

    void seenProperty()
    {
        Picking p;
        p.history.add(mace);
        QVERIFY(p.refreshFromSite());
        const QString chaos = u"[Weapon] Chaos DPS"_s;
        QCOMPARE(p.of(chaos, StatPicker::NameRole).toString(), u"Chaos DPS"_s);
        QCOMPARE(
            p.of(chaos, StatPicker::NoteRole).toString(),
            u"The weapon's own, with its mods. Not on the trade site. On one of the items you've checked"_s);
        QVERIFY(p.keys().indexOf(chaos) < properties.size());
        QCOMPARE(p.picker.rowCount(), properties.size() + 5);
    }

    void seenWithoutTheSite()
    {
        Picking p;
        p.history.add(ring);
        p.history.add(helmet);
        p.picker.refresh();
        QCOMPARE(p.keys().mid(properties.size()), QStringList({life, rarity}));
        QCOMPARE(p.of(life, StatPicker::NoteRole).toString(), u"On 2 of the items you've checked"_s);
        QCOMPARE(p.of(rarity, StatPicker::NoteRole).toString(), u"On one of the items you've checked"_s);
    }

    void search()
    {
        Picking p;
        QVERIFY(p.refreshFromSite());
        QSignalSpy queryChanged(&p.picker, &StatPicker::queryChanged);
        p.picker.setQuery(u"fire res"_s);
        QCOMPARE(p.keys(), QStringList({fire}));
        p.picker.setQuery(u"  RESIST  fIRE "_s);
        QCOMPARE(p.keys(), QStringList({fire}));
        p.picker.setQuery(u"armour"_s);
        QCOMPARE(p.keys(), QStringList({u"[Base] Armour"_s, armour}));
        p.picker.setQuery(u"fire life"_s);
        QCOMPARE(p.keys(), QStringList());
        QCOMPARE(queryChanged.size(), 4);

        // the same words
        QSignalSpy reset(&p.picker, &StatPicker::modelReset);
        p.picker.setQuery(u"fire  life"_s);
        QCOMPARE(reset.size(), 0);
        QCOMPARE(p.picker.query(), u"fire  life"_s);
        QCOMPARE(queryChanged.size(), 5);
        p.picker.setQuery(u"fire  life"_s);
        QCOMPARE(queryChanged.size(), 5);

        // whatever the space is
        p.picker.setQuery(u"fire\t res"_s);
        QCOMPARE(p.keys(), QStringList({fire}));

        p.picker.setQuery(QString());
        QCOMPARE(p.picker.rowCount(), properties.size() + 4);
    }

    void searchKept()
    {
        Picking p;
        p.picker.setQuery(u"fire"_s);
        QVERIFY(p.refreshFromSite());
        QCOMPARE(p.keys(), QStringList({fire}));
    }

    void taken()
    {
        Picking p;
        QVERIFY(p.refreshFromSite());
        QSignalSpy dataChanged(&p.picker, &StatPicker::dataChanged);
        QSignalSpy takenChanged(&p.picker, &StatPicker::takenChanged);
        p.picker.setTaken({life, u"[Base] Spirit"_s});
        QVERIFY(p.of(life, StatPicker::TakenRole).toBool());
        QVERIFY(p.of(u"[Base] Spirit"_s, StatPicker::TakenRole).toBool());
        QVERIFY(!p.of(fire, StatPicker::TakenRole).toBool());
        QCOMPARE(dataChanged.size(), 1);
        QCOMPARE(dataChanged.at(0).at(0).toModelIndex().row(), 0);
        QCOMPARE(dataChanged.at(0).at(1).toModelIndex().row(), p.picker.rowCount() - 1);
        QCOMPARE(dataChanged.at(0).at(2).value<QList<int>>(), QList<int>({StatPicker::TakenRole}));
        QCOMPARE(takenChanged.size(), 1);

        p.picker.setTaken({life, u"[Base] Spirit"_s});
        QCOMPARE(takenChanged.size(), 1);
        QCOMPARE(p.picker.taken(), QStringList({life, u"[Base] Spirit"_s}));
    }

    void takenWithNothingListed()
    {
        Picking p;
        QSignalSpy dataChanged(&p.picker, &StatPicker::dataChanged);
        QSignalSpy takenChanged(&p.picker, &StatPicker::takenChanged);
        p.picker.setTaken({life});
        QCOMPARE(takenChanged.size(), 1);
        QCOMPARE(dataChanged.size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestStatPicker)
#include "tst_statpicker.moc"
