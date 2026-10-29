// SPDX-License-Identifier: MIT
#include "app/traderesults.h"
#include "core/parser.h"
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "resultsite.h"
#include "services/ratesstore.h"
#include "services/tradeclient.h"
#include "services/tradesearcher.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using namespace resultsite;

class TestTradeResults : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what they say when the site lets them down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.trade.warning=false\n"
                                         u"pathofupgrading.trade.info=false\n"
                                         u"pathofupgrading.ninja.debug=false"_s);
    }

    void startsEmpty()
    {
        Site site;
        QVERIFY(!site.results.hasSearched());
        QCOMPARE(site.results.rowCount(), 0);
        QVERIFY(!site.results.isBusy());
        QVERIFY(!site.results.hasMore());
        QCOMPARE(site.results.waiting(), 0);
        QVERIFY(site.results.error().isEmpty());
        // and nothing to ask for
        site.results.ask();
        site.results.loadMore();
        QVERIFY(!site.results.isBusy());
        QVERIFY(site.network.sent.isEmpty());
    }

    void takesASearchAndAsksForNothing()
    {
        Site site;
        QSignalSpy changed(&site.results, &TradeResults::searchChanged);
        QSignalSpy more(&site.results, &TradeResults::moreChanged);
        const TradeFinder::Found found = foundBy(250);
        site.results.setSearch(found);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(more.size(), 1);

        QVERIFY(site.results.hasSearched());
        QCOMPARE(site.results.title(), u"Same stats or better than Doom Visage"_s);
        QCOMPARE(site.results.league(), league);
        QCOMPARE(site.results.total(), 250);
        // of which the site lists the first hundred
        QCOMPARE(site.results.available(), 100);
        QCOMPARE(site.results.url(), found.url);
        QVERIFY(site.results.hasMore());
        QCOMPARE(site.results.rowCount(), 0);
        QVERIFY(!site.results.isBusy());
        // however long it's looked at
        QTest::qWait(30);
        QVERIFY(site.network.sent.isEmpty());
        QVERIFY(site.ninja.sent.isEmpty());
    }

    void asksForTheFirstPage()
    {
        Site site;
        const TradeFinder::Found found = foundBy(250);
        site.network.answers = {fetched(found.listingIds.first(10))};
        site.results.setSearch(found);

        QSignalSpy busy(&site.results, &TradeResults::busyChanged);
        QSignalSpy waiting(&site.results, &TradeResults::waitingChanged);
        site.results.ask();
        QVERIFY(site.results.isBusy());
        QCOMPARE(busy.size(), 1);
        QVERIFY(site.waitsForIt());
        QCOMPARE(busy.size(), 2);
        // it didn't have to wait
        QVERIFY(waiting.isEmpty());

        QCOMPARE(site.results.rowCount(), 10);
        QVERIFY(site.results.hasMore());
        QVERIFY(site.results.error().isEmpty());
        QCOMPARE(site.network.sent.size(), 1);
        QCOMPARE(site.network.sent.first().request.url(), fetchUrl(found.listingIds.first(10)));

        // it has them now
        site.results.ask();
        QVERIFY(!site.results.isBusy());
        QTest::qWait(30);
        QCOMPARE(site.network.sent.size(), 1);
    }

    void loadsTheRest()
    {
        Site site;
        const TradeFinder::Found found = foundBy(25);
        site.network.answers = {fetched(found.listingIds.first(10)), fetched(found.listingIds.mid(10, 10)),
                                fetched(found.listingIds.sliced(20))};
        site.results.setSearch(found);

        QSignalSpy inserted(&site.results, &QAbstractItemModel::rowsInserted);
        QSignalSpy more(&site.results, &TradeResults::moreChanged);
        site.results.ask();
        QVERIFY(site.waitsForIt());
        QCOMPARE(site.results.rowCount(), 10);
        QVERIFY(site.results.hasMore());
        QVERIFY(site.loads());
        QCOMPARE(site.results.rowCount(), 20);
        QVERIFY(site.results.hasMore());
        QVERIFY(site.loads());
        QCOMPARE(site.results.rowCount(), 25);
        QVERIFY(!site.results.hasMore());
        QCOMPARE(more.size(), 3);

        // each page below the ones before it
        QCOMPARE(inserted.size(), 3);
        QCOMPARE(inserted.at(0).at(1).toInt(), 0);
        QCOMPARE(inserted.at(0).at(2).toInt(), 9);
        QCOMPARE(inserted.at(1).at(1).toInt(), 10);
        QCOMPARE(inserted.at(2).at(1).toInt(), 20);
        QCOMPARE(inserted.at(2).at(2).toInt(), 24);
        QCOMPARE(site.network.sent.size(), 3);
        QCOMPARE(site.network.sent.at(1).request.url(), fetchUrl(found.listingIds.mid(10, 10)));
        QCOMPARE(site.network.sent.at(2).request.url(), fetchUrl(found.listingIds.sliced(20)));

        // and no more to ask for
        site.results.loadMore();
        QVERIFY(!site.results.isBusy());
        QCOMPARE(site.network.sent.size(), 3);
    }

    // a page is only asked for once at a time
    void doesNotAskTwice()
    {
        Site site;
        const TradeFinder::Found found = foundBy(25);
        site.network.answers = {fetched(found.listingIds.first(10)), fetched(found.listingIds.mid(10, 10))};
        site.results.setSearch(found);
        site.results.ask();
        site.results.loadMore();
        site.results.ask();
        QVERIFY(site.waitsForIt());
        QCOMPARE(site.network.sent.size(), 1);
        QCOMPARE(site.results.rowCount(), 10);
    }

    // the site gives the ids of the first hundred, whatever it found
    void stopsAtTheIdsItHas()
    {
        Site site;
        const TradeFinder::Found found = foundBy(5000, u"Ab3dE9"_s, 23);
        site.network.answers = {fetched(found.listingIds.first(10)), fetched(found.listingIds.mid(10, 10)),
                                fetched(found.listingIds.sliced(20))};
        site.results.setSearch(found);
        QCOMPARE(site.results.total(), 5000);
        for (int rows : {10, 20, 23}) {
            QVERIFY(site.results.hasMore());
            QVERIFY(site.loads());
            QCOMPARE(site.results.rowCount(), rows);
        }
        QVERIFY(!site.results.hasMore());
        QCOMPARE(site.results.total(), 5000);
        site.results.loadMore();
        QVERIFY(!site.results.isBusy());
        QCOMPARE(site.network.sent.size(), 3);
    }

    void showsTheListings()
    {
        Site site;
        const QString indexed = QDateTime::currentDateTimeUtc().addSecs(-5 * 60).toString(Qt::ISODate);
        QJsonArray result;
        result << helmet(u"x1"_s, price(3, u"divine"_s), indexed);
        result << helmet(u"x2"_s, price(50, u"exalted"_s), {}, {});
        result << helmet(u"x3"_s, {}, {}, u"Iron Hat"_s);
        site.network.answers = {{.body = json({{u"result"_s, result}})}};
        site.results.setSearch(foundBy(3));
        QVERIFY(site.loads());
        QCOMPARE(site.results.rowCount(), 3);
        // it's a list
        QCOMPARE(site.results.rowCount(site.results.index(0)), 0);

        using Role = TradeResults;
        QCOMPARE(site.text(0, Role::PriceRole), u"3 div"_s);
        QCOMPARE(site.text(0, Role::AccountRole), u"Sellerx1#1234"_s);
        QVERIFY(site.results.data(site.results.index(0), Role::OnlineRole).toBool());
        QCOMPARE(site.text(0, Role::ListedRole), u"5 minutes ago"_s);
        QCOMPARE(site.text(0, Role::NameRole), u"Grim Ward"_s);
        QCOMPARE(site.text(0, Role::BaseRole), u"Iron Hat"_s);
        const QList<ResultMod> mods =
            site.results.data(site.results.index(0), Role::ModsRole).value<QList<ResultMod>>();
        QCOMPARE(mods, (QList<ResultMod>{{u"+80 to maximum Life"_s, u"explicit"_s},
                                         {u"Cannot be Frozen"_s, u"explicit"_s}}));

        // the other prices, and no time or name
        QCOMPARE(site.text(1, Role::PriceRole), u"50 ex"_s);
        QCOMPARE(site.text(1, Role::ListedRole), QString());
        QCOMPARE(site.text(1, Role::NameRole), u"Iron Hat"_s);
        QCOMPARE(site.text(1, Role::BaseRole), QString());
        QCOMPARE(site.text(2, Role::PriceRole), u"no price"_s);
        // a base type that is its name is shown once
        QCOMPARE(site.text(2, Role::NameRole), u"Iron Hat"_s);
        QCOMPARE(site.text(2, Role::BaseRole), QString());

        const QHash<int, QByteArray> names = site.results.roleNames();
        QCOMPARE(names.value(Role::PriceRole), "price"_ba);
        QCOMPARE(names.value(Role::ConvertedRole), "converted"_ba);
        QCOMPARE(names.value(Role::AccountRole), "account"_ba);
        QCOMPARE(names.value(Role::OnlineRole), "online"_ba);
        QCOMPARE(names.value(Role::ListedRole), "listed"_ba);
        QCOMPARE(names.value(Role::NameRole), "name"_ba);
        QCOMPARE(names.value(Role::BaseRole), "base"_ba);
        QCOMPARE(names.value(Role::ModsRole), "mods"_ba);
    }

    void convertsPrices()
    {
        Site site;
        QJsonArray result;
        result << helmet(u"x1"_s, price(3, u"divine"_s));
        result << helmet(u"x2"_s, price(1000, u"exalted"_s));
        result << helmet(u"x3"_s, price(50, u"exalted"_s));
        result << helmet(u"x4"_s, price(4, u"chaos"_s));
        result << helmet(u"x5"_s, price(2, u"mystery"_s));
        result << helmet(u"x6"_s, {});
        site.network.answers = {{.body = json({{u"result"_s, result}})}};
        site.results.setSearch(foundBy(6));
        QVERIFY(site.loads());

        // there are no rates yet
        for (int row = 0; row < 6; ++row)
            QCOMPARE(site.text(row, TradeResults::ConvertedRole), QString());

        // and they change what's shown when they come
        QSignalSpy changed(&site.results, &QAbstractItemModel::dataChanged);
        QVERIFY(site.hasRates(700));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().at(0).toModelIndex().row(), 0);
        QCOMPARE(changed.first().at(1).toModelIndex().row(), 5);
        QCOMPARE(changed.first().at(2).value<QList<int>>(), (QList<int>{TradeResults::ConvertedRole}));
        QCOMPARE(site.text(0, TradeResults::ConvertedRole), u"about 2100 ex"_s);
        QCOMPARE(site.text(1, TradeResults::ConvertedRole), u"about 1.4 div"_s);
        // nothing when it says as much as the price, or there's no rate
        QCOMPARE(site.text(2, TradeResults::ConvertedRole), QString());
        QCOMPARE(site.text(3, TradeResults::ConvertedRole), u"about 40 ex"_s);
        QCOMPARE(site.text(4, TradeResults::ConvertedRole), QString());
        QCOMPARE(site.text(5, TradeResults::ConvertedRole), QString());
    }

    // the rates it has might be for another league
    void convertsOnlyAtTheRatesLeague()
    {
        Site site;
        site.network.answers = {fetched({u"x1"_s})};
        site.results.setSearch(foundBy(1));
        QVERIFY(site.loads());
        QVERIFY(site.hasRates(700));
        QCOMPARE(site.text(0, TradeResults::ConvertedRole), u"about 2100 ex"_s);

        // after a search in another league, and after searches ran in that one
        TradeFinder::Found other = foundBy(1, u"Zz9"_s);
        other.league = u"HC Runes of Aldur"_s;
        site.results.setSearch(other);
        site.network.answers = {fetched({u"x1"_s})};
        QVERIFY(site.loads());
        QCOMPARE(site.rates.league(), league);
        QCOMPARE(site.text(0, TradeResults::ConvertedRole), QString());
    }

    void convertsOnceTheRatesComeLaterToo()
    {
        Site site;
        site.network.answers = {fetched({u"x1"_s})};
        site.results.setSearch(foundBy(1));
        QSignalSpy changed(&site.results, &QAbstractItemModel::dataChanged);
        QVERIFY(site.hasRates(700));
        // with nothing to change
        QVERIFY(changed.isEmpty());
        QVERIFY(site.loads());
        QCOMPARE(site.text(0, TradeResults::ConvertedRole), u"about 2100 ex"_s);
    }

    void leavesOutTheSold()
    {
        Site site;
        const TradeFinder::Found found = foundBy(25);
        site.network.answers = {fetched(found.listingIds.first(10), {u"x2"_s, u"x9"_s}),
                                fetched(found.listingIds.mid(10, 10))};
        site.results.setSearch(found);
        QVERIFY(site.loads());
        QCOMPARE(site.results.rowCount(), 8);
        // they were asked for, so the next page is the next ten
        QVERIFY(site.loads());
        QCOMPARE(site.results.rowCount(), 18);
        QCOMPARE(site.network.sent.at(1).request.url(), fetchUrl(found.listingIds.mid(10, 10)));
    }

    // where all of the first page is sold, the next is still to ask for
    void allOfAPageSold()
    {
        Site site;
        const TradeFinder::Found found = foundBy(15);
        const QStringList first = found.listingIds.first(10);
        site.network.answers = {fetched(first, first)};
        site.results.setSearch(found);
        QSignalSpy inserted(&site.results, &QAbstractItemModel::rowsInserted);
        site.results.ask();
        QVERIFY(site.waitsForIt());
        QCOMPARE(site.results.rowCount(), 0);
        QVERIFY(site.results.error().isEmpty());
        QVERIFY(site.results.hasMore());
        QVERIFY(inserted.isEmpty());
        // and asking to see them again doesn't go back for that page
        site.results.ask();
        QVERIFY(!site.results.isBusy());
    }

    void aSearchWithNothingListed()
    {
        Site site;
        site.results.setSearch(foundBy(0));
        QVERIFY(site.results.hasSearched());
        QCOMPARE(site.results.total(), 0);
        QVERIFY(!site.results.hasMore());
        site.results.ask();
        site.results.loadMore();
        QVERIFY(!site.results.isBusy());
        QTest::qWait(20);
        QVERIFY(site.network.sent.isEmpty());
    }

    void aNewSearchReplacesThem()
    {
        Site site;
        const TradeFinder::Found first = foundBy(25);
        const TradeFinder::Found second = foundBy(12, u"Qw8rT2"_s);
        site.network.answers = {fetched(first.listingIds.first(10)), fetched(second.listingIds.first(10))};
        site.results.setSearch(first);
        QVERIFY(site.loads());
        QCOMPARE(site.results.rowCount(), 10);

        QSignalSpy reset(&site.results, &QAbstractItemModel::modelReset);
        QSignalSpy changed(&site.results, &TradeResults::searchChanged);
        site.results.setSearch(second);
        QCOMPARE(reset.size(), 1);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(site.results.rowCount(), 0);
        QCOMPARE(site.results.total(), 12);
        QCOMPARE(site.results.url(), second.url);
        QVERIFY(site.results.hasMore());
        // which waits to be asked for as well
        QTest::qWait(20);
        QCOMPARE(site.network.sent.size(), 1);

        site.results.ask();
        QVERIFY(site.waitsForIt());
        QCOMPARE(site.results.rowCount(), 10);
        QCOMPARE(site.network.sent.last().request.url(), fetchUrl(second.listingIds.first(10), u"Qw8rT2"_s));
    }

    void aNewSearchCallsOffTheRequest()
    {
        Site site;
        const TradeFinder::Found first = foundBy(25);
        site.network.answers = {fetched(first.listingIds.first(10))};
        site.results.setSearch(first);
        site.results.ask();
        QVERIFY(site.results.isBusy());

        QSignalSpy busy(&site.results, &TradeResults::busyChanged);
        site.results.setSearch(foundBy(3, u"Qw8rT2"_s));
        QVERIFY(!site.results.isBusy());
        QCOMPARE(busy.size(), 1);
        // so what it was asking for doesn't turn up in the new ones
        QTest::qWait(30);
        QCOMPARE(site.results.rowCount(), 0);
        QVERIFY(site.results.hasMore());
        QCOMPARE(busy.size(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(site.client.findChildren<TradeReply *>().isEmpty());
    }

    void keepsWhatItHasWhenAPageFails()
    {
        Site site;
        const TradeFinder::Found found = foundBy(25);
        site.network.answers = {
            fetched(found.listingIds.first(10)), {.status = 500}, fetched(found.listingIds.mid(10, 10))};
        site.results.setSearch(found);
        QVERIFY(site.loads());

        QSignalSpy errors(&site.results, &TradeResults::errorChanged);
        QVERIFY(site.loads());
        QCOMPARE(site.results.rowCount(), 10);
        QCOMPARE(site.results.error(), u"The trade site's answer couldn't be read (HTTP 500)."_s);
        QCOMPARE(errors.size(), 1);
        // and can ask for it again
        QVERIFY(site.results.hasMore());
        QVERIFY(site.loads());
        QCOMPARE(site.results.rowCount(), 20);
        QVERIFY(site.results.error().isEmpty());
        QCOMPARE(errors.size(), 2);
        QCOMPARE(site.network.sent.last().request.url(), fetchUrl(found.listingIds.mid(10, 10)));
    }

    void asksAgainForTheFirstPageAfterAFailure()
    {
        Site site;
        const TradeFinder::Found found = foundBy(25);
        site.network.answers = {{.status = 500}, fetched(found.listingIds.first(10))};
        site.results.setSearch(found);
        site.results.ask();
        QVERIFY(site.waitsForIt());
        QCOMPARE(site.results.rowCount(), 0);
        QVERIFY(!site.results.error().isEmpty());
        QVERIFY(site.results.hasMore());

        site.results.ask();
        QVERIFY(site.results.isBusy());
        QVERIFY(site.results.error().isEmpty());
        QVERIFY(site.waitsForIt());
        QCOMPARE(site.results.rowCount(), 10);
    }

    void aNewSearchClearsTheError()
    {
        Site site;
        site.network.answers = {{.status = 500}};
        site.results.setSearch(foundBy(25));
        site.results.ask();
        QVERIFY(site.waitsForIt());
        QVERIFY(!site.results.error().isEmpty());

        QSignalSpy errors(&site.results, &TradeResults::errorChanged);
        site.results.setSearch(foundBy(5, u"Qw8rT2"_s));
        QVERIFY(site.results.error().isEmpty());
        QCOMPARE(errors.size(), 1);
    }

    void asksNothingWhileTradeIsOff_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<bool>("soloSelfFound");
        QTest::addColumn<QString>("why");

        QTest::newRow("off") << false << false << u"Trade searches are turned off in the settings."_s;
        QTest::newRow("solo self-found")
            << true << true << u"There's no trading in solo self-found, so trade searches are off."_s;
    }

    void asksNothingWhileTradeIsOff()
    {
        QFETCH(bool, enabled);
        QFETCH(bool, soloSelfFound);
        QFETCH(QString, why);
        Site site;
        site.results.setSearch(foundBy(25));
        TradeSettings settings;
        settings.enabled = enabled;
        settings.league = {.name = league, .soloSelfFound = soloSelfFound};
        site.searcher.setSettings(settings);

        QSignalSpy errors(&site.results, &TradeResults::errorChanged);
        site.results.ask();
        QVERIFY(!site.results.isBusy());
        QCOMPARE(site.results.error(), why);
        QCOMPARE(errors.size(), 1);
        site.results.loadMore();
        QCOMPARE(errors.size(), 1);
        QTest::qWait(30);
        QVERIFY(site.network.sent.isEmpty());
        QVERIFY(site.ninja.sent.isEmpty());
        QCOMPARE(site.results.rowCount(), 0);

        // and they come once searches are back on
        site.network.answers = {fetched(listingIds(10))};
        site.searcher.setSettings(TradeSettings{.league = {.name = league}});
        site.results.ask();
        QVERIFY(site.results.error().isEmpty());
        QVERIFY(site.waitsForIt());
        QCOMPARE(site.results.rowCount(), 10);
    }

    // turned off after some were loaded, the rest aren't asked for
    void asksNoMoreOnceTradeIsOff()
    {
        Site site;
        site.network.answers = {fetched(listingIds(10))};
        site.results.setSearch(foundBy(25));
        QVERIFY(site.loads());
        site.searcher.setSettings(TradeSettings{.enabled = false});
        site.results.loadMore();
        QVERIFY(!site.results.isBusy());
        QCOMPARE(site.results.error(), u"Trade searches are turned off in the settings."_s);
        QCOMPARE(site.results.rowCount(), 10);
        QCOMPARE(site.network.sent.size(), 1);
    }

    // the site says one request a second, so the third page waits for it
    void saysHowLongItWaits()
    {
        Site site;
        FakeAnswer oneASecond = fetched(listingIds(10));
        oneASecond.headers = {{"X-Rate-Limit-Ip", "1:1:0"}};
        site.network.answers = {oneASecond, fetched(listingIds(20).mid(10)), fetched(listingIds(25).mid(20))};
        site.results.setSearch(foundBy(25));
        QVERIFY(site.loads());
        QVERIFY(site.loads());
        QCOMPARE(site.results.waiting(), 0);

        QSignalSpy waiting(&site.results, &TradeResults::waitingChanged);
        site.results.loadMore();
        // a second and the little more that it adds
        QTRY_COMPARE(site.results.waiting(), 2);
        QCOMPARE(waiting.size(), 1);
        QVERIFY(site.results.isBusy());
        QCOMPARE(site.results.rowCount(), 20);

        // and is done with it when the site takes the request
        QTRY_VERIFY_WITH_TIMEOUT(!site.results.isBusy(), 3000);
        QCOMPARE(site.results.waiting(), 0);
        QCOMPARE(waiting.size(), 2);
        QCOMPARE(site.results.rowCount(), 25);
    }

    // four in ten seconds, until the site says how many
    void aNewSearchEndsTheWait()
    {
        Site site;
        site.results.setSearch(foundBy(60));
        for (int page = 0; page < 4; ++page)
            QVERIFY(site.loads());
        QCOMPARE(site.results.waiting(), 0);

        QSignalSpy waiting(&site.results, &TradeResults::waitingChanged);
        site.results.loadMore();
        QVERIFY(site.results.isBusy());
        QTRY_VERIFY(site.results.waiting() > 0);
        QCOMPARE_LE(site.results.waiting(), 11);
        QCOMPARE(waiting.size(), 1);
        QCOMPARE(site.network.sent.size(), 4);

        site.results.setSearch(foundBy(5, u"Qw8rT2"_s));
        QVERIFY(!site.results.isBusy());
        QCOMPARE(site.results.waiting(), 0);
        QCOMPARE(waiting.size(), 2);
    }

    void callsOffWhatItAsksForWhenItGoes()
    {
        Site site;
        {
            TradeResults results(&site.searcher, &site.rates);
            results.setSearch(foundBy(25));
            results.ask();
            QVERIFY(results.isBusy());
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(site.client.findChildren<TradeReply *>().isEmpty());
        QTest::qWait(20);
    }
};

QTEST_GUILESS_MAIN(TestTradeResults)
#include "tst_traderesults.moc"
