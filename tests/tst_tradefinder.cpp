// SPDX-License-Identifier: MIT
#include "app/tradefinder.h"
#include "core/datafiles.h"
#include "core/parser.h"
#include "core/tradequery.h"
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "itemtexts.h"
#include "services/tradeclient.h"
#include "services/tradedatastore.h"
#include "services/tradesearcher.h"
#include "tradefixtures.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

using Kind = TradeFinder::Kind;

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

const Item helmet = parseItem(itemtexts::helmetNew).value();

const QStringList siteLeagues = {u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s};

QJsonObject leagues()
{
    QJsonArray result;
    for (const QString &id : siteLeagues)
        result << QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}};
    return {{u"result"_s, result}};
}

// What the site answers when it has run a search.
FakeAnswer found(const QString &id, int total, const QStringList &ids = {})
{
    return {.body =
                json({{u"id"_s, id}, {u"total"_s, total}, {u"result"_s, QJsonArray::fromStringList(ids)}})};
}

// The trade site, and searches on it in the current league.
struct Site
{
    QTemporaryDir dir;
    FakeNetwork network;
    TradeClient client{&network};
    TradeDataStore data{&client, dir.path()};
    TradeSearcher searcher{&client};
    TradeFinder finder{&searcher, &data};

    Site() { searcher.setLeagues(siteLeagues); }

    // what the site said of its stats earlier today, which the store reads
    bool hasData()
    {
        for (const auto &[name, answer] :
             {std::pair{u"stats"_s, tradefixtures::stats()},
              std::pair{u"filters"_s, tradefixtures::filters()}, std::pair{u"leagues"_s, leagues()}}) {
            if (!writeJsonFile(dir.filePath(name + u".json"_s), answer))
                return false;
        }
        QSignalSpy refreshed(&data, &TradeDataStore::refreshed);
        data.refresh();
        return refreshed.wait() && network.sent.isEmpty();
    }

    // once it has finished, which it does once
    bool finds(const Item &item, Kind kind = Kind::Similar)
    {
        QSignalSpy finished(&finder, &TradeFinder::finished);
        finder.find(item, kind);
        return finished.wait() && !finder.isBusy() && !finished.wait(10) && finished.size() == 1;
    }

    // the nth search it sent
    QJsonObject searched(qsizetype n) const
    {
        return QJsonDocument::fromJson(network.sent.value(n).body).object();
    }
};

// A rare with a mod that armour has a local version of.
Item withArmour(const QString &itemClass, const QString &base)
{
    return parseItem(u"Item Class: %1\nRarity: Rare\nGrim Ward\n%2\n--------\nItem Level: 80\n--------\n"
                     u"40% increased Armour"_s.arg(itemClass, base))
        .value();
}

SimilarSearch similar(const Item &item, const TradeData &data, int level = 0)
{
    SimilarSearchOptions options;
    options.level = level;
    // armour has local mods
    options.preferLocal = true;
    return similarSearch(item, data, options);
}

} // namespace

class TestTradeFinder : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what it says when the site lets it down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.trade.warning=false"_s);
    }

    void init() { QTest::failOnWarning(); }

    void sameOrBetter()
    {
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 12, {u"a"_s, u"b"_s})};
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.searched(0), similar(helmet, site.data.data()).body);
        QVERIFY(site.finder.errorString().isEmpty());

        const std::optional<TradeFinder::Found> &found = site.finder.found();
        QVERIFY(found);
        QCOMPARE(found->kind, Kind::Similar);
        QCOMPARE(found->item.name, u"Doom Visage"_s);
        QCOMPARE(found->title, u"Same stats or better than Doom Visage"_s);
        QCOMPARE(found->note, QString());
        QCOMPARE(found->league, u"Runes of Aldur"_s);
        QCOMPARE(found->url, tradeResultsUrl(u"Runes of Aldur"_s, u"s1"_s));
        QCOMPARE(found->searchId, u"s1"_s);
        QCOMPARE(found->total, 12);
        QCOMPARE(found->listingIds, (QStringList{u"a"_s, u"b"_s}));
    }

    void yours()
    {
        // the listings you want, without the stats your character ignores
        Site site;
        QVERIFY(site.hasData());
        Settings settings;
        settings.trade.status = ListingStatus::Online;
        site.finder.setSettings(settings);
        Character character;
        character.ignored = {{u"res"_s, {}, IgnoreRule::Mode::Exclude}};
        site.finder.setCharacter(character);
        QVERIFY(site.finds(helmet));

        SimilarSearchOptions options;
        options.status = ListingStatus::Online;
        options.ignoreRules = character.ignored;
        options.preferLocal = true;
        QCOMPARE(site.searched(0), similarSearch(helmet, site.data.data(), options).body);
    }

    void localMods()
    {
        // on armour, and not on jewellery
        Site site;
        QVERIFY(site.hasData());
        QVERIFY(site.finds(withArmour(u"Helmets"_s, u"Visored Helm"_s)));
        QVERIFY(site.network.sent.at(0).body.contains("explicit.stat_armour_local"));
        QVERIFY(site.finds(withArmour(u"Amulets"_s, u"Gold Amulet"_s)));
        QVERIFY(site.network.sent.at(1).body.contains("explicit.stat_armour\""));
    }

    void baseOnly()
    {
        // which has its armour to search for
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 4)};
        QVERIFY(site.finds(parseItem(u"Item Class: Helmets\nRarity: Normal\nExpert Visored Helm\n--------\n"
                                     u"Armour: 312\n--------\nItem Level: 79"_s)
                               .value()));
        QVERIFY(site.finder.errorString().isEmpty());
        QCOMPARE(site.finder.found()->total, 4);
    }

    void unique()
    {
        // which is found by its name once nothing has its stats
        const Item belt =
            parseItem(
                u"Item Class: Belts\nRarity: Unique\nMageblood\nHeavy Belt\n--------\n+50 to maximum Life"_s)
                .value();
        Site site;
        QVERIFY(site.hasData());
        for (int level = 0; level < similarSearchLevels() - 1; ++level)
            site.network.answers << found(u"s%1"_s.arg(level), 0);
        site.network.answers << found(u"all"_s, 8);
        QVERIFY(site.finds(belt));

        SimilarSearchOptions last;
        last.level = similarSearchLevels() - 1;
        QCOMPARE(site.searched(similarSearchLevels() - 1), similarSearch(belt, site.data.data(), last).body);
        QCOMPARE(site.finder.found()->title, u"Same stats or better than Mageblood"_s);
        QCOMPARE(site.finder.found()->note,
                 u"Nothing matched its stats, so these are all the Mageblood listed."_s);
        QCOMPARE(site.finder.found()->total, 8);
    }

    void busy()
    {
        Site site;
        QVERIFY(site.hasData());
        QVERIFY(!site.finder.isBusy());
        site.finder.find(helmet, Kind::Similar);
        QVERIFY(site.finder.isBusy());
        QSignalSpy finished(&site.finder, &TradeFinder::finished);
        QVERIFY(finished.wait());
        QVERIFY(!site.finder.isBusy());
    }

    void looser()
    {
        // when nothing has your values
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 0), found(u"s2"_s, 3, {u"a"_s})};
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.network.sent.size(), 2);
        const SimilarSearch looser = similar(helmet, site.data.data(), 1);
        QCOMPARE(site.searched(1), looser.body);
        QCOMPARE(site.finder.found()->searchId, u"s2"_s);
        QCOMPARE(
            site.finder.found()->note,
            u"Nothing matched your exact values, so these have 90% of your values, on %1 of the %2 stats."_s
                .arg(looser.need)
                .arg(looser.total));

        // and the next starts with your values again
        site.network.answers = {found(u"s3"_s, 1)};
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.searched(2), similar(helmet, site.data.data()).body);
        QCOMPARE(site.finder.found()->note, QString());
    }

    void forgetsTheLast()
    {
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 1)};
        QVERIFY(site.finds(helmet));
        site.searcher.setSettings({.enabled = false});
        QVERIFY(site.finds(helmet));
        QVERIFY(!site.finder.found());

        site.searcher.setSettings({});
        site.network.answers = {found(u"s2"_s, 1)};
        QVERIFY(site.finds(helmet));
        QVERIFY(site.finder.errorString().isEmpty());
    }

    void nothingListed()
    {
        Site site;
        QVERIFY(site.hasData());
        for (int level = 0; level < similarSearchLevels(); ++level)
            site.network.answers << found(u"s%1"_s.arg(level), 0);
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.network.sent.size(), similarSearchLevels());
        QCOMPARE(site.finder.found()->total, 0);
        QVERIFY(site.finder.errorString().isEmpty());
    }

    void turnedOff()
    {
        // and the site isn't asked for its stats either
        Site site;
        site.searcher.setSettings({.enabled = false});
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.finder.errorString(), u"Trade searches are turned off in the settings."_s);
        QVERIFY(!site.finder.found());
        QVERIFY(site.network.sent.isEmpty());
    }

    void waitsForStats()
    {
        Site site;
        site.network.answers = {{.body = json(tradefixtures::stats())},
                                {.body = json(tradefixtures::filters())},
                                {.body = json(leagues())},
                                found(u"s1"_s, 5)};
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.network.sent.size(), 4);
        QCOMPARE(site.searched(3), similar(helmet, site.data.data()).body);
        QCOMPARE(site.finder.found()->total, 5);

        // and once they're in, it waits no more
        QSignalSpy refreshed(&site.data, &TradeDataStore::refreshed);
        site.data.refresh();
        QVERIFY(refreshed.wait());
        QCOMPARE(site.network.sent.size(), 4);
    }

    void noStats()
    {
        Site site;
        const FakeAnswer unreachable{.error = QNetworkReply::HostNotFoundError};
        site.network.answers = {unreachable, unreachable, unreachable};
        QVERIFY(site.finds(helmet));
        QVERIFY(site.finder.errorString().startsWith(u"Searches need the trade site's list of stats."_s));
        QCOMPARE(site.finder.errorString(), site.data.errorString());
        QCOMPARE(site.network.sent.size(), 3);
    }

    void nothingToSearch()
    {
        Site site;
        QVERIFY(site.hasData());
        QVERIFY(site.finds(
            parseItem(u"Item Class: Rings\nRarity: Normal\nGold Ring\n--------\nItem Level: 50"_s).value()));
        QCOMPARE(site.finder.errorString(),
                 u"None of this item's stats could be matched on the trade site."_s);
        QVERIFY(site.network.sent.isEmpty());
    }

    void siteSaysNo()
    {
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {
            {.status = 400,
             .body = json({{u"error"_s, QJsonObject{{u"message"_s, u"Unknown item base"_s}}}})}};
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.finder.errorString(), u"The trade site says: Unknown item base"_s);
        QVERIFY(!site.finder.found());
    }

    void askedAgain()
    {
        // before the first has finished, which is called off
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 1), found(u"s2"_s, 2)};
        site.finder.find(helmet, Kind::Similar);
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.finder.found()->searchId, u"s2"_s);
        QVERIFY(!site.network.sent.at(0).reply);
    }

    void calledOff()
    {
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 1)};
        QSignalSpy finished(&site.finder, &TradeFinder::finished);
        site.finder.find(helmet, Kind::Similar);
        site.finder.cancel();
        QVERIFY(!site.finder.isBusy());
        QVERIFY(!finished.wait(50));
        QVERIFY(!site.network.sent.at(0).reply);
    }

    void gone()
    {
        // before the site answers
        Site site;
        QVERIFY(site.hasData());
        auto finder = std::make_unique<TradeFinder>(&site.searcher, &site.data);
        finder->find(helmet, Kind::Similar);
        QVERIFY(site.network.sent.at(0).reply);
        finder.reset();
        QVERIFY(!site.network.sent.at(0).reply);
    }

    void calledOffSoon()
    {
        // before it says why it can't
        Site site;
        site.searcher.setSettings({.enabled = false});
        QSignalSpy finished(&site.finder, &TradeFinder::finished);
        site.finder.find(helmet, Kind::Similar);
        site.finder.cancel();
        QVERIFY(!finished.wait(50));
    }

    void calledOffWhileWaiting()
    {
        // for the stats, which come in anyway
        Site site;
        site.network.answers = {{.body = json(tradefixtures::stats())},
                                {.body = json(tradefixtures::filters())},
                                {.body = json(leagues())}};
        QSignalSpy finished(&site.finder, &TradeFinder::finished);
        site.finder.find(helmet, Kind::Similar);
        site.finder.cancel();
        QSignalSpy refreshed(&site.data, &TradeDataStore::refreshed);
        QVERIFY(refreshed.wait());
        QVERIFY(!site.data.data().stats.isEmpty());
        QVERIFY(!finished.wait(50));
        QCOMPARE(site.network.sent.size(), 3);
    }
};

QTEST_GUILESS_MAIN(TestTradeFinder)
#include "tst_tradefinder.moc"
