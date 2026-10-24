// SPDX-License-Identifier: MIT
#include "app/tradefinder.h"
#include "core/buildprofiles.h"
#include "core/datafiles.h"
#include "core/evaluation.h"
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
#include <array>
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

// What the site says of a search with more filters than it takes. It
// takes plenty of searches, so that they aren't held.
FakeAnswer tooManyFilters()
{
    return {.status = 400,
            .body = json({{u"error"_s, QJsonObject{{u"message"_s, u"Query is too complex."_s}}}}),
            .headers = {{"X-Rate-Limit-Ip", "30:10:0"}}};
}

// So many trade stats for life that each smaller search leaves some out,
// and none for the other stats a helmet wants.
QJsonObject manyStats()
{
    using namespace tradefixtures;
    QJsonArray explicits;
    for (int n = 1; n <= 24; ++n)
        explicits << stat(u"explicit.stat_life_%1"_s.arg(n), u"+# to maximum Life"_s, u"explicit"_s);
    const QJsonArray implicits = {stat(u"implicit.stat_life"_s, u"+# to maximum Life"_s, u"implicit"_s)};
    return response({group(u"explicit"_s, explicits), group(u"implicit"_s, implicits)});
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
    bool hasData(const QJsonObject &stats = tradefixtures::stats())
    {
        for (const auto &[name, answer] :
             {std::pair{u"stats"_s, stats}, std::pair{u"filters"_s, tradefixtures::filters()},
              std::pair{u"leagues"_s, leagues()}}) {
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

Item ring(const QString &name, const QString &mods)
{
    return parseItem(u"Item Class: Rings\nRarity: Rare\n"_s + name + u"\nGold Ring\n--------\n"_s + mods)
        .value();
}

const Item belt =
    parseItem(u"Item Class: Belts\nRarity: Unique\nMageblood\nHeavy Belt\n--------\n+50 to maximum Life"_s)
        .value();

// Someone in their old helmet.
Character inGrimCrown()
{
    Character character;
    character.gear = {{u"Helmet"_s, parseItem(itemtexts::helmetOld).value()}};
    return character;
}

// How a listing compares with yours.
struct Upgrade
{
    double share;
    double baseShare;
};
constexpr Upgrade clearlyBetter{1.06, 0.9};
constexpr Upgrade littleBetter{1, 0.85};
constexpr Upgrade nearlyAsGood{0.9, 0.75};

// The smaller searches for when the site says one is too complex, from 1.
GearSearchOptions smaller(int size)
{
    GearSearchOptions options;
    options.explicitOnly = true;
    options.maxFilters = std::array{40, 20, 10, 6, 6}.at(size);
    if (size >= 2)
        options.keyStatShare = 0;
    options.countOnly = size == 4;
    return options;
}

// What it should search for to find upgrades in the slot.
GearSearch upgrades(const Item &item, const TradeData &data, const Character &character, const QString &slot,
                    Upgrade upgrade = clearlyBetter, const Settings &settings = {},
                    GearSearchOptions options = {})
{
    options.status = settings.trade.status;
    options.ignoreRules = character.ignored;
    options.share = upgrade.share;
    options.baseShare = upgrade.baseShare;
    const PobPriorities &priorities = settings.pob.priorities;
    return gearSearch(item, searchWeights(item.slot, gearScoreOptions(character, slot, priorities)), data,
                      options);
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

    void betterForYourBuild()
    {
        Site site;
        QVERIFY(site.hasData());
        const Character character = inGrimCrown();
        site.finder.setCharacter(character);
        site.network.answers = {found(u"s1"_s, 9, {u"a"_s})};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.searched(0), upgrades(helmet, site.data.data(), character, u"Helmet"_s).body);
        QVERIFY(site.finder.errorString().isEmpty());

        const TradeFinder::Found &found = *site.finder.found();
        QCOMPARE(found.kind, Kind::Upgrades);
        QCOMPARE(found.title, u"Better for your build than Doom Visage (gear score %1)"_s.arg(
                                  evaluate(helmet, character).gear->score));
        QCOMPARE(found.note, u"Scored by your build's weights for Armour at least 280, Life, and Fire res. "
                             u"Each has at least 2 of "
                             u"these at a decent roll: Life, Fire res. Best for your build first."_s);
        QCOMPARE(found.total, 9);
        QCOMPARE(found.listingIds, QStringList{u"a"_s});
    }

    void upgradesLooser()
    {
        const Character character = inGrimCrown();
        Site site;
        QVERIFY(site.hasData());
        site.finder.setCharacter(character);
        site.network.answers = {found(u"s1"_s, 0), found(u"s2"_s, 2)};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.searched(1),
                 upgrades(helmet, site.data.data(), character, u"Helmet"_s, littleBetter).body);
        QCOMPARE(site.finder.found()->note,
                 u"No clear upgrade is listed right now, so these are a little better than yours."_s);

        // on a site that hasn't been searched yet, so it's not held back
        Site again;
        QVERIFY(again.hasData());
        again.finder.setCharacter(character);
        again.network.answers = {found(u"s1"_s, 0), found(u"s2"_s, 0), found(u"s3"_s, 1)};
        QVERIFY(again.finds(helmet, Kind::Upgrades));
        QCOMPARE(again.searched(2),
                 upgrades(helmet, again.data.data(), character, u"Helmet"_s, nearlyAsGood).body);
        QCOMPARE(again.finder.found()->note,
                 u"Nothing better is listed right now, so these come within 10% of yours."_s);
    }

    void noUpgradesListed()
    {
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 0), found(u"s2"_s, 0), found(u"s3"_s, 0)};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.network.sent.size(), 3);
        QCOMPARE(site.finder.found()->total, 0);
        QVERIFY(site.finder.errorString().isEmpty());
    }

    void ringItWouldReplace()
    {
        // the weaker of yours, whose fire resistance goes with it, by what
        // you put first
        Character character;
        character.gear = {
            {u"Ring 1"_s, ring(u"Storm Loop"_s, u"+30% to Fire Resistance\n+60 to maximum Life"_s)},
            {u"Ring 2"_s, ring(u"Dull Loop"_s, u"+10% to Fire Resistance\n+10 to maximum Life"_s)}};
        ResistEntry entry;
        entry.values[Element::Fire] = 70;
        entry.buffer = 10;
        character.resists = snapshotResists(entry, character.gear, ResistSource::Manual);
        Settings settings;
        settings.pob.priorities.resistsFirst = false;
        const PobPriorities &priorities = settings.pob.priorities;
        const Item mine = ring(u"Grim Loop"_s, u"+40% to Fire Resistance\n+20 to maximum Life"_s);
        const Evaluation evaluation = evaluate(mine, character, priorities);
        QCOMPARE(evaluation.comparison.targetSlot, u"Ring 2"_s);
        QVERIFY(evaluation.gear->score != evaluate(mine, character).gear->score);

        Site site;
        QVERIFY(site.hasData());
        site.finder.setSettings(settings);
        site.finder.setCharacter(character);
        site.network.answers = {found(u"s1"_s, 1)};
        QVERIFY(site.finds(mine, Kind::Upgrades));
        const TradeData &data = site.data.data();
        QCOMPARE(site.searched(0),
                 upgrades(mine, data, character, u"Ring 2"_s, clearlyBetter, settings).body);
        QVERIFY(site.searched(0) !=
                upgrades(mine, data, character, u"Ring 1"_s, clearlyBetter, settings).body);
        QVERIFY(site.searched(0) != upgrades(mine, data, character, u"Ring"_s, clearlyBetter, settings).body);
        QVERIFY(site.searched(0) != upgrades(mine, data, character, u"Ring 2"_s).body);
        QCOMPARE(site.finder.found()->title,
                 u"Better for your build than Grim Loop (gear score %1)"_s.arg(evaluation.gear->score));
    }

    void uniqueUpgrades()
    {
        // have their stats, as they can't roll others
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 1)};
        QVERIFY(site.finds(belt, Kind::Upgrades));
        QCOMPARE(site.searched(0), similarSearch(belt, site.data.data()).body);
        QCOMPARE(site.finder.found()->title, u"Same stats or better than Mageblood"_s);
    }

    void upgradesWithoutABuildProfile()
    {
        Site site;
        QVERIFY(site.hasData());
        Character character = inGrimCrown();
        character.buildProfile = u"none of them"_s;
        site.finder.setCharacter(character);
        site.network.answers = {found(u"s1"_s, 1)};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.searched(0), similar(helmet, site.data.data()).body);
        QCOMPARE(site.finder.found()->title, u"Same stats or better than Doom Visage"_s);
    }

    void whatItLooksAt()
    {
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 1), found(u"s2"_s, 1)};
        QVERIFY(
            site.finds(parseItem(u"Item Class: Boots\nRarity: Rare\nGrim Stride\nExpert Sandals\n--------\n"
                                 u"Armour: 120\nEvasion Rating: 110\nEnergy Shield: 40\n--------\n"
                                 u"Item Level: 80\n--------\n+50 to maximum Life\n+20% to Fire Resistance\n"
                                 u"25% increased Movement Speed"_s)
                           .value(),
                       Kind::Upgrades));
        QCOMPARE(
            site.finder.found()->note,
            u"Scored by your build's weights for Armour at least 108, Evasion Rating at least 99, Energy "
            u"Shield at least 36, Life, Fire res, and 1 more. Each has at least 2 of these at a decent roll: "
            u"Movement speed, Life, Fire res. Best for your build first."_s);

        // and when yours has none of it
        QVERIFY(site.finds(
            parseItem(u"Item Class: Rings\nRarity: Normal\nGold Ring\n--------\nItem Level: 50"_s).value(),
            Kind::Upgrades));
        QCOMPARE(site.finder.found()->note, u"Scored by your build's weights. Best for your build first."_s);

        // or one of the stats it has to have
        site.network.answers = {found(u"s3"_s, 1)};
        QVERIFY(site.finds(ring(u"Grim Loop"_s, u"+50 to maximum Life"_s), Kind::Upgrades));
        QCOMPARE(site.finder.found()->note, u"Scored by your build's weights for Life. Each has at least 1 "
                                            u"of these at a decent roll: Life, Fire res. "
                                            u"Best for your build first."_s);
    }

    void yourUpgrades()
    {
        // the listings you want, without the stats your character ignores
        Settings settings;
        settings.trade.status = ListingStatus::Online;
        Character character = inGrimCrown();
        character.ignored = {{u"res"_s, {}, IgnoreRule::Mode::Exclude}};
        Site site;
        QVERIFY(site.hasData());
        site.finder.setSettings(settings);
        site.finder.setCharacter(character);
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.searched(0),
                 upgrades(helmet, site.data.data(), character, u"Helmet"_s, clearlyBetter, settings).body);
    }

    void flasksAndCharms()
    {
        // which have no gear score
        const Item flask =
            parseItem(u"Item Class: Life Flasks\nRarity: Magic\nUltimate Life Flask of the Doctor"
                      u"\n--------\nItem Level: 60\n--------\n+50 to maximum Life"_s)
                .value();
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 1)};
        QVERIFY(site.finds(flask, Kind::Upgrades));
        QCOMPARE(site.searched(0), similarSearch(flask, site.data.data()).body);
    }

    void unidentified()
    {
        // which anything rolled beats
        const Item mystery =
            parseItem(
                u"Item Class: Helmets\nRarity: Rare\nExpert Visored Helm\n--------\nArmour: 312\n--------\n"
                u"Item Level: 79\n--------\nUnidentified"_s)
                .value();
        QVERIFY(mystery.unidentified);
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {found(u"s1"_s, 1)};
        QVERIFY(site.finds(mystery, Kind::Upgrades));
        QCOMPARE(site.searched(0), upgrades(mystery, site.data.data(), {}, u"Helmet"_s).body);
        QCOMPARE(site.finder.found()->title, u"Better for your build than Expert Visored Helm"_s);
    }

    void nothingWanted()
    {
        // by the build profile, once you turned all of it down
        Character character = inGrimCrown();
        for (const ProfileStat &stat : buildProfile({})->stats)
            character.tweaks.insert(stat.label, 0);
        Site site;
        QVERIFY(site.hasData());
        site.finder.setCharacter(character);
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.finder.errorString(),
                 u"Your build profile wants none of the stats this slot can have."_s);
        QVERIFY(site.network.sent.isEmpty());
    }

    void noneOnTheSite()
    {
        // of the stats your build wants
        Site site;
        const QJsonArray odd = {
            tradefixtures::stat(u"explicit.stat_light"_s, u"#% increased Light Radius"_s, u"explicit"_s)};
        QVERIFY(site.hasData(tradefixtures::response({tradefixtures::group(u"explicit"_s, odd)})));
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.finder.errorString(),
                 u"None of your build's stats for this slot could be matched on the trade site."_s);
        QVERIFY(site.network.sent.isEmpty());
    }

    void tooComplex()
    {
        // so it's made smaller
        const Character character = inGrimCrown();
        Site site;
        QVERIFY(site.hasData());
        site.finder.setCharacter(character);
        site.network.answers = {tooManyFilters(), found(u"s1"_s, 3)};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.network.sent.size(), 2);
        const TradeData &data = site.data.data();
        QCOMPARE(site.searched(0), upgrades(helmet, data, character, u"Helmet"_s).body);
        QCOMPARE(site.searched(1),
                 upgrades(helmet, data, character, u"Helmet"_s, clearlyBetter, {}, smaller(1)).body);
        QCOMPARE(site.finder.found()->total, 3);
        QCOMPARE(site.finder.found()->note,
                 u"Scored by your build's main weights, as the trade site limits how many filters a search "
                 u"can have. Best for your build first."_s);

        // and the next starts there, and can be made smaller again
        site.network.answers = {tooManyFilters(), found(u"s2"_s, 1)};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.network.sent.size(), 4);
        for (int size : {1, 2})
            QCOMPARE(site.searched(size + 1),
                     upgrades(helmet, data, character, u"Helmet"_s, clearlyBetter, {}, smaller(size)).body);
        QCOMPARE(site.finder.found()->searchId, u"s2"_s);
    }

    void smallerAndLooser()
    {
        const Character character = inGrimCrown();
        Site site;
        QVERIFY(site.hasData());
        site.finder.setCharacter(character);
        site.network.answers = {tooManyFilters(), found(u"s1"_s, 0), found(u"s2"_s, 2)};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(
            site.searched(2),
            upgrades(helmet, site.data.data(), character, u"Helmet"_s, littleBetter, {}, smaller(1)).body);
        QCOMPARE(site.finder.found()->note,
                 u"No clear upgrade is listed right now, so these are a little better than yours."_s);
    }

    void tooComplexAtItsSmallest()
    {
        const Character character = inGrimCrown();
        Site site;
        QVERIFY(site.hasData(manyStats()));
        site.finder.setCharacter(character);
        site.network.answers = {tooManyFilters(), tooManyFilters(), tooManyFilters(), tooManyFilters(),
                                tooManyFilters()};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.network.sent.size(), 5);
        // each smaller than the last
        for (int size = 1; size < 5; ++size) {
            const GearSearch search =
                upgrades(helmet, site.data.data(), character, u"Helmet"_s, clearlyBetter, {}, smaller(size));
            QCOMPARE(site.searched(size), search.body);
            QVERIFY2(search.body != site.searched(size - 1), QByteArray::number(size).constData());
        }
        QCOMPARE(site.finder.errorString(),
                 u"The trade site says this search has too many filters even at its smallest."_s);
        QVERIFY(!site.finder.found());

        // which may find nothing, and isn't any looser
        site.network.answers = {found(u"s1"_s, 0)};
        QVERIFY(site.finds(helmet, Kind::Upgrades));
        QCOMPARE(site.network.sent.size(), 6);
        QCOMPARE(site.searched(5), site.searched(4));
        QCOMPARE(site.finder.found()->total, 0);
        QCOMPARE(site.finder.found()->note,
                 u"The trade site only takes a simple search, so each has at least 1 of these at a decent "
                 u"roll: Life. Cheapest first."_s);
    }

    void similarTooComplex()
    {
        // which isn't made smaller
        Site site;
        QVERIFY(site.hasData());
        site.network.answers = {tooManyFilters()};
        QVERIFY(site.finds(helmet));
        QCOMPARE(site.network.sent.size(), 1);
        QCOMPARE(site.finder.errorString(), u"The trade site says the search has too many filters."_s);
    }

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
