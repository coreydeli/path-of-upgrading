// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/recommendedupgrades.h"
#include "app/settingsstore.h"
#include "app/tradefinder.h"
#include "app/traderesults.h"
#include "core/datafiles.h"
#include "core/parser.h"
#include "fakenetwork.h"
#include "itemtexts.h"
#include "resultsite.h"
#include "services/pobengine.h"
#include "services/ratesstore.h"
#include "services/tradeclient.h"
#include "services/tradedatastore.h"
#include "services/tradesearcher.h"
#include "tradefixtures.h"

#include <QAbstractItemModelTester>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using resultsite::league;
using resultsite::siteLeagues;

namespace {

Item item(const QString &itemClass, const QString &name, const QString &mods)
{
    return parseItem(
               u"Item Class: %1\nRarity: Rare\n%2\nIron Base\n--------\nItem Level: 80\n--------\n%3"_s.arg(
                   itemClass, name, mods))
        .value();
}

// What a finished item has all of that a slot looks for.
const QString plenty = u"+90 to maximum Life\n20% increased Movement Speed\n+30% to Fire Resistance\n"
                       "+30% to Cold Resistance\n+30% to Lightning Resistance"_s;

// Someone with everything but an offhand well covered, and an old helmet.
Character wearingOldHelmet()
{
    Character c;
    c.name = u"Tester"_s;
    c.gear = {
        {u"Weapon"_s, item(u"One Hand Swords"_s, u"Doom Edge"_s, plenty)},
        {u"Helmet"_s, parseItem(itemtexts::helmetOld).value()},
        {u"Body Armour"_s, item(u"Body Armours"_s, u"Doom Plate"_s, plenty)},
        {u"Gloves"_s, item(u"Gloves"_s, u"Doom Grip"_s, plenty)},
        {u"Boots"_s, item(u"Boots"_s, u"Doom Step"_s, plenty)},
        {u"Belt"_s, item(u"Belts"_s, u"Doom Sash"_s, plenty)},
        {u"Amulet"_s, item(u"Amulets"_s, u"Doom Eye"_s, plenty)},
        {u"Ring 1"_s, item(u"Rings"_s, u"Doom Band"_s, plenty)},
        {u"Ring 2"_s, item(u"Rings"_s, u"Doom Loop"_s, plenty)},
    };
    return c;
}

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

FakeAnswer searchFound(const QString &id, int total)
{
    return {.body = json({{u"id"_s, id},
                          {u"total"_s, total},
                          {u"result"_s, QJsonArray::fromStringList(resultsite::listingIds(total))}})};
}

QJsonObject leaguesAnswer()
{
    QJsonArray result;
    for (const QString &id : siteLeagues)
        result << QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}};
    return {{u"result"_s, result}};
}

// The character you play, the trade site, and the recommendations.
struct Site
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    CharacterStore store{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    FakeNetwork network;
    TradeClient client{&network};
    TradeDataStore data{&client, dir.path()};
    TradeSearcher searcher{&client};
    FakeNetwork ninja;
    RatesStore rates{&ninja, dir.filePath(u"rates"_s)};
    TradeResults results{&searcher, &rates};
    RecommendedUpgrades upgrades{&active, &settings, &searcher, &data, &rates, &results};
    QAbstractItemModelTester tester{&upgrades, QAbstractItemModelTester::FailureReportingMode::QtTest};

    Site()
    {
        searcher.setLeagues(siteLeagues);
        upgrades.setTradeAvailable(true);
        // signed in, so that upgrade searches start at their full size
        upgrades.finder().setSignedIn(true);
    }

    void play(const Character &character) { active.setId(store.add(character).value_or(QString())); }

    // what the site said of its stats earlier today, which the store reads
    bool hasData()
    {
        for (const auto &[name, answer] :
             {std::pair{u"stats"_s, tradefixtures::stats()},
              std::pair{u"filters"_s, tradefixtures::filters()}, std::pair{u"leagues"_s, leaguesAnswer()}}) {
            if (!writeJsonFile(dir.filePath(name + u".json"_s), answer))
                return false;
        }
        QSignalSpy refreshed(&data, &TradeDataStore::refreshed);
        data.refresh();
        return refreshed.wait() && network.sent.isEmpty();
    }

    int rowOf(const QString &slot) const
    {
        for (int row = 0; row < upgrades.rowCount(); ++row) {
            if (upgrades.index(row).data(RecommendedUpgrades::SlotRole).toString() == slot)
                return row;
        }
        return -1;
    }

    QVariant in(const QString &slot, RecommendedUpgrades::Role role) const
    {
        const int row = rowOf(slot);
        return row < 0 ? QVariant() : upgrades.index(row).data(role);
    }

    TradeReport tradeOf(const QString &slot) const
    {
        return in(slot, RecommendedUpgrades::TradeRole).value<TradeReport>();
    }

    // until its search has been answered
    bool waitsForIt(const QString &slot)
    {
        QSignalSpy changed(&upgrades, &RecommendedUpgrades::dataChanged);
        while (tradeOf(slot).busy) {
            if (!changed.wait())
                return false;
        }
        return true;
    }
};

} // namespace

class TestRecommendedUpgrades : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void initTestCase()
    {
        QLoggingCategory::setFilterRules(u"pathofupgrading.trade.warning=false\n"
                                         u"pathofupgrading.ninja.warning=false\n"
                                         u"pathofupgrading.pob.info=false"_s);
    }

    void nobodyPlayed()
    {
        Site site;
        // there are slots to fill, with what the build wants in them
        QCOMPARE(site.upgrades.rowCount(), 3);
        QCOMPARE(site.in(u"Offhand"_s, RecommendedUpgrades::SlotRole).toString(), u"Offhand"_s);
        QCOMPARE(site.in(u"Offhand"_s, RecommendedUpgrades::NameRole).toString(), QString());
        QCOMPARE(site.in(u"Offhand"_s, RecommendedUpgrades::ScoreRole).toString(), QString());
        QVERIFY(!site.in(u"Offhand"_s, RecommendedUpgrades::LookForRole).toString().isEmpty());
        QVERIFY(!site.tradeOf(u"Offhand"_s).asked);
    }

    void rowsFollowWhatIsWorn()
    {
        Site site;
        site.play(wearingOldHelmet());
        // the empty offhand, then the one worn slot with something left to find
        QCOMPARE(site.upgrades.rowCount(), 2);
        QCOMPARE(site.upgrades.index(0).data(RecommendedUpgrades::SlotRole).toString(), u"Offhand"_s);
        QCOMPARE(site.in(u"Helmet"_s, RecommendedUpgrades::NameRole).toString(), u"Grim Crown"_s);
        QCOMPARE(site.in(u"Helmet"_s, RecommendedUpgrades::BaseRole).toString(), u"Visored Helm"_s);
        QVERIFY(!site.in(u"Helmet"_s, RecommendedUpgrades::ScoreRole).toString().isEmpty());
        QVERIFY(!site.in(u"Helmet"_s, RecommendedUpgrades::GradeRole).toString().isEmpty());
        // it has the 55 life asked for, so that's left out
        QCOMPARE(site.in(u"Helmet"_s, RecommendedUpgrades::LookForRole).toString(),
                 u"Fire res 24 or more (up to 45), Cold res 24 or more (up to 45), "
                 u"Lightning res 24 or more (up to 45)"_s);

        // and the offhand is covered once there is one, and the list follows
        QSignalSpy reset(&site.upgrades, &QAbstractItemModel::modelReset);
        QVERIFY(site.active.wear(u"Offhand"_s, item(u"Shields"_s, u"Doom Ward"_s, plenty)));
        QCOMPARE(reset.size(), 1);
        QCOMPARE(site.upgrades.rowCount(), 1);
        QCOMPARE(site.upgrades.index(0).data(RecommendedUpgrades::SlotRole).toString(), u"Helmet"_s);
    }

    void tradeAvailability()
    {
        Site site;
        QSignalSpy changed(&site.upgrades, &RecommendedUpgrades::tradeAvailableChanged);
        site.upgrades.setTradeAvailable(true);
        QCOMPARE(changed.size(), 0);
        site.upgrades.setTradeAvailable(false);
        QCOMPARE(changed.size(), 1);
        QVERIFY(!site.upgrades.isTradeAvailable());
    }

    void searchesForUpgradesToTheWornItem()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        site.network.answers = {searchFound(u"Ab3dE9"_s, 3)};
        const int row = site.rowOf(u"Helmet"_s);
        QSignalSpy changed(&site.upgrades, &RecommendedUpgrades::dataChanged);
        site.upgrades.find(row);
        // it says so at once
        QCOMPARE(site.tradeOf(u"Helmet"_s).status, u"Searching the trade site"_s);
        QVERIFY(site.tradeOf(u"Helmet"_s).busy);
        QVERIFY(site.waitsForIt(u"Helmet"_s));

        const TradeReport report = site.tradeOf(u"Helmet"_s);
        QCOMPARE(report.status, u"3 listed in Runes of Aldur"_s);
        QVERIFY(report.listed);
        QVERIFY(!report.failed);
        QVERIFY(report.title.contains(u"Grim Crown"_s));
        QVERIFY(report.url.toString().endsWith(u"/Ab3dE9"_s));
        // the other row has nothing to say
        QVERIFY(!site.tradeOf(u"Offhand"_s).asked);

        // for the helmet, as the popup would search for it
        QCOMPARE(site.network.sent.size(), 1);
        TradeFinder reference(&site.searcher, &site.data, &site.rates);
        reference.setSignedIn(true);
        reference.setCharacter(site.active.character());
        QSignalSpy finished(&reference, &TradeFinder::finished);
        site.network.answers = {searchFound(u"Zz9"_s, 1)};
        reference.find(site.active.character().gear.value(u"Helmet"_s), TradeFinder::Kind::Upgrades);
        QVERIFY(finished.wait());
        QCOMPARE(site.network.sent.size(), 2);
        QCOMPARE(site.network.sent.at(0).body, site.network.sent.at(1).body);
    }

    void nothingListed()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        // not even the looser searches find any
        site.network.answers = {searchFound(u"a"_s, 0), searchFound(u"b"_s, 0), searchFound(u"c"_s, 0),
                                searchFound(u"d"_s, 0), searchFound(u"e"_s, 0), searchFound(u"f"_s, 0)};
        site.upgrades.find(site.rowOf(u"Helmet"_s));
        QVERIFY(site.waitsForIt(u"Helmet"_s));
        QVERIFY(site.tradeOf(u"Helmet"_s).status.startsWith(u"None listed"_s));
        QVERIFY(!site.tradeOf(u"Helmet"_s).listed);

        // so there is nothing to show
        QSignalSpy asked(&site.upgrades, &RecommendedUpgrades::resultsAsked);
        site.upgrades.showResults(site.rowOf(u"Helmet"_s));
        QCOMPARE(asked.size(), 0);
    }

    void anEmptySlotHasNothingToSearch()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        site.upgrades.find(site.rowOf(u"Offhand"_s));
        QVERIFY(!site.tradeOf(u"Offhand"_s).asked);
        // and a row that isn't there
        site.upgrades.find(-1);
        site.upgrades.find(99);
        QVERIFY(site.network.sent.isEmpty());
    }

    void searchesFailWhenTradeIsOff()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        site.searcher.setSettings(TradeSettings{.enabled = false});
        site.upgrades.find(site.rowOf(u"Helmet"_s));
        QVERIFY(site.waitsForIt(u"Helmet"_s));
        const TradeReport report = site.tradeOf(u"Helmet"_s);
        QVERIFY(report.failed);
        QCOMPARE(report.status, tradeUnavailable({.enabled = false}));
        QVERIFY(site.network.sent.isEmpty());
    }

    void failsWhenTheSiteIsDown()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        site.network.answers = {{.error = QNetworkReply::HostNotFoundError}};
        site.upgrades.find(site.rowOf(u"Helmet"_s));
        QVERIFY(site.waitsForIt(u"Helmet"_s));
        QVERIFY(site.tradeOf(u"Helmet"_s).failed);
        QVERIFY(!site.tradeOf(u"Helmet"_s).status.isEmpty());
    }

    void waitsItsTurn()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        // the site takes four searches in ten seconds until it says otherwise
        for (int i = 0; i < 4; ++i)
            site.searcher.search({{u"query"_s, QJsonObject()}});
        QSignalSpy changed(&site.upgrades, &RecommendedUpgrades::dataChanged);
        site.upgrades.find(site.rowOf(u"Helmet"_s));
        while (!site.tradeOf(u"Helmet"_s).note.contains(u"few searches"_s))
            QVERIFY(changed.wait());
        const TradeReport report = site.tradeOf(u"Helmet"_s);
        QVERIFY(report.busy);
        QVERIFY(report.status.startsWith(u"Waiting "_s));
        QVERIFY(report.status.endsWith(u" seconds for the trade site"_s));
        site.upgrades.finder().cancel();
    }

    void anotherSearchCallsOffTheLast()
    {
        Site site;
        QVERIFY(site.hasData());
        Character c = wearingOldHelmet();
        c.gear.insert(u"Boots"_s, item(u"Boots"_s, u"Doom Step"_s, u"+30 to maximum Life"_s));
        site.play(c);
        QCOMPARE(site.upgrades.rowCount(), 3);

        // the site hasn't answered about the boots when the helmet is searched
        site.network.answers = {{.never = true}};
        site.upgrades.find(site.rowOf(u"Boots"_s));
        QVERIFY(site.tradeOf(u"Boots"_s).busy);
        site.network.answers = {searchFound(u"Second"_s, 2)};
        site.upgrades.find(site.rowOf(u"Helmet"_s));
        QVERIFY(site.waitsForIt(u"Helmet"_s));
        QCOMPARE(site.tradeOf(u"Helmet"_s).status, u"2 listed in Runes of Aldur"_s);
        QVERIFY(!site.tradeOf(u"Boots"_s).asked);
    }

    void aChangeNothingToItKeepsTheSearch()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        site.network.answers = {{.never = true}};
        site.upgrades.find(site.rowOf(u"Helmet"_s));
        QVERIFY(site.tradeOf(u"Helmet"_s).busy);

        // the same character saved again, as when Path of Building has loaded it
        QSignalSpy reset(&site.upgrades, &QAbstractItemModel::modelReset);
        site.active.reload();
        QCOMPARE(reset.size(), 0);
        QVERIFY(site.tradeOf(u"Helmet"_s).busy);

        // but wearing something else calls it off
        QVERIFY(site.active.wear(u"Helmet"_s, item(u"Helmets"_s, u"Dusk Crown"_s, u"+10 to maximum Life"_s)));
        QCOMPARE(reset.size(), 1);
        QVERIFY(!site.tradeOf(u"Helmet"_s).asked);
    }

    void showsWhatItFound()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        site.network.answers = {searchFound(u"Ab3dE9"_s, 3)};
        const int row = site.rowOf(u"Helmet"_s);
        site.upgrades.find(row);
        QVERIFY(site.waitsForIt(u"Helmet"_s));
        // the results window isn't touched until you ask
        QVERIFY(!site.results.hasSearched());

        QSignalSpy asked(&site.upgrades, &RecommendedUpgrades::resultsAsked);
        site.upgrades.showResults(row);
        QCOMPARE(asked.size(), 1);
        QVERIFY(site.results.hasSearched());
        QCOMPARE(site.results.total(), 3);
        QCOMPARE(site.results.url(), site.tradeOf(u"Helmet"_s).url);

        // another search may have taken their place since
        site.results.setSearch(resultsite::foundBy(7, u"Other1"_s));
        site.upgrades.showResults(row);
        QCOMPARE(asked.size(), 2);
        QCOMPARE(site.results.total(), 3);

        // and a row with no search shows nothing
        site.upgrades.showResults(site.rowOf(u"Offhand"_s));
        site.upgrades.showResults(-1);
        QCOMPARE(asked.size(), 2);
    }

    void aChosenSlotSaysWhatItCanRoll()
    {
        Site site;
        site.play(wearingOldHelmet());
        QSignalSpy changed(&site.upgrades, &RecommendedUpgrades::chosenChanged);
        QVERIFY(site.upgrades.chosenCanRoll().isEmpty());

        // a worn slot, even one with nothing left to look for
        site.upgrades.choose(u"Boots"_s);
        QVERIFY(site.upgrades.chosenLookFor().isEmpty());
        const QStringList boots = site.upgrades.chosenCanRoll();
        QVERIFY(!boots.isEmpty());
        QVERIFY(boots.first().contains(u", item level "_s));

        // an empty slot with a class
        site.upgrades.choose(u"Belt"_s);
        QVERIFY(!site.upgrades.chosenCanRoll().isEmpty());
        QVERIFY(site.upgrades.chosenCanRoll() != boots);

        // the weapon and the offhand have no class to go by, and nor does nothing
        site.upgrades.choose(u"Weapon"_s);
        QVERIFY(site.upgrades.chosenCanRoll().isEmpty());
        site.upgrades.choose(u"Offhand"_s);
        QVERIFY(site.upgrades.chosenCanRoll().isEmpty());
        site.upgrades.choose(QString());
        QVERIFY(site.upgrades.chosenCanRoll().isEmpty());
    }

    void aChosenSlotSaysWhatToLookFor()
    {
        Site site;
        site.play(wearingOldHelmet());
        QSignalSpy changed(&site.upgrades, &RecommendedUpgrades::chosenChanged);
        QCOMPARE(site.upgrades.chosen(), QString());
        QCOMPARE(site.upgrades.chosenLookFor(), QString());
        QVERIFY(!site.upgrades.chosenWorn());

        // any slot, not only those that are recommended
        site.upgrades.choose(u"Helmet"_s);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(site.upgrades.chosen(), u"Helmet"_s);
        QVERIFY(site.upgrades.chosenWorn());
        QCOMPARE(site.upgrades.chosenLookFor(),
                 site.in(u"Helmet"_s, RecommendedUpgrades::LookForRole).toString());
        site.upgrades.choose(u"Helmet"_s);
        QCOMPARE(changed.size(), 1);

        // an empty slot has what the build wants in it
        site.upgrades.choose(u"Offhand"_s);
        QVERIFY(!site.upgrades.chosenWorn());
        QCOMPARE(site.upgrades.chosenLookFor(),
                 site.in(u"Offhand"_s, RecommendedUpgrades::LookForRole).toString());
        QVERIFY(!site.upgrades.chosenLookFor().isEmpty());

        // one that has all it needs has nothing to look for
        site.upgrades.choose(u"Boots"_s);
        QVERIFY(site.upgrades.chosenWorn());
        QCOMPARE(site.upgrades.chosenLookFor(), QString());

        // a slot that doesn't exist isn't picked, and empty puts it back
        site.upgrades.choose(u"Tail"_s);
        QCOMPARE(site.upgrades.chosen(), u"Boots"_s);
        site.upgrades.choose(QString());
        QCOMPARE(site.upgrades.chosen(), QString());
        QCOMPARE(site.upgrades.chosenLookFor(), QString());
    }

    void aTwoHandedWeaponLeavesTheOffhandAlone()
    {
        Site site;
        Character c = wearingOldHelmet();
        c.gear.insert(u"Weapon"_s, item(u"Two Hand Maces"_s, u"Doom Maul"_s, plenty));
        site.play(c);
        QVERIFY(c.gear.value(u"Weapon"_s).twoHanded);
        site.upgrades.choose(u"Offhand"_s);
        QCOMPARE(site.upgrades.chosenLookFor(), QString());
    }

    void searchesTheChosenSlot()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        QSignalSpy changed(&site.upgrades, &RecommendedUpgrades::chosenChanged);
        // the boots aren't one of the recommendations
        QCOMPARE(site.rowOf(u"Boots"_s), -1);
        site.upgrades.choose(u"Boots"_s);
        site.network.answers = {searchFound(u"Bt4"_s, 2)};
        site.upgrades.findChosen();
        QVERIFY(site.upgrades.chosenTrade().busy);
        QCOMPARE(site.upgrades.chosenTrade().status, u"Searching the trade site"_s);
        QTRY_VERIFY(!site.upgrades.chosenTrade().busy);
        QCOMPARE(site.upgrades.chosenTrade().status, u"2 listed in Runes of Aldur"_s);
        QVERIFY(changed.size() >= 2);

        // and shows what it found
        QSignalSpy asked(&site.upgrades, &RecommendedUpgrades::resultsAsked);
        site.upgrades.showChosenResults();
        QCOMPARE(asked.size(), 1);
        QCOMPARE(site.results.total(), 2);

        // another slot has its own report
        site.upgrades.choose(u"Helmet"_s);
        QVERIFY(!site.upgrades.chosenTrade().asked);
        site.upgrades.choose(u"Boots"_s);
        QVERIFY(site.upgrades.chosenTrade().listed);

        // there's nothing to search in an empty slot
        site.upgrades.choose(u"Offhand"_s);
        const qsizetype sent = site.network.sent.size();
        site.upgrades.findChosen();
        QVERIFY(!site.upgrades.chosenTrade().asked);
        QCOMPARE(site.network.sent.size(), sent);
    }

    void aSearchEndsWhenTheItemIsChanged()
    {
        Site site;
        QVERIFY(site.hasData());
        site.play(wearingOldHelmet());
        site.upgrades.choose(u"Boots"_s);
        site.network.answers = {{.never = true}};
        site.upgrades.findChosen();
        QVERIFY(site.upgrades.chosenTrade().busy);
        QSignalSpy changed(&site.upgrades, &RecommendedUpgrades::chosenChanged);

        // the same item saved again keeps it
        site.active.reload();
        QVERIFY(site.upgrades.chosenTrade().busy);

        // another one in the slot is not what it searched for, though the
        // recommendations stay as they were
        QSignalSpy reset(&site.upgrades, &QAbstractItemModel::modelReset);
        QVERIFY(site.active.wear(u"Boots"_s, item(u"Boots"_s, u"Dusk Step"_s, plenty)));
        QCOMPARE(reset.size(), 0);
        QVERIFY(!site.upgrades.chosenTrade().asked);
        QVERIFY(!site.upgrades.chosenTrade().busy);
        QVERIFY(changed.size() >= 1);
        QCOMPARE(site.upgrades.chosen(), u"Boots"_s);

        // and the look for follows what's worn
        QCOMPARE(site.upgrades.chosenLookFor(), QString());
        QVERIFY(site.active.wear(u"Boots"_s, item(u"Boots"_s, u"Ash Step"_s, u"+10 to maximum Life"_s)));
        QVERIFY(!site.upgrades.chosenLookFor().isEmpty());
    }
};

QTEST_MAIN(TestRecommendedUpgrades)
#include "tst_recommendedupgrades.moc"
