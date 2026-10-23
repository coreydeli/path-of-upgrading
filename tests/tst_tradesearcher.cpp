// SPDX-License-Identifier: MIT
#include "core/tradequery.h"
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "services/tradeclient.h"
#include "services/tradesearcher.h"
#include "tradefixtures.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

namespace {

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// What the site answers when it won't run a search.
FakeAnswer siteError(const QString &message)
{
    return {.status = 400,
            .body = json({{u"error"_s, QJsonObject{{u"code"_s, 2}, {u"message"_s, message}}}})};
}

// What the site answers when it has run one.
FakeAnswer found(const QString &id, int total, const QStringList &ids)
{
    const QJsonObject answer{
        {u"id"_s, id},
        {u"total"_s, total},
        {u"result"_s, QJsonArray::fromStringList(ids)},
    };
    return {.body = json(answer)};
}

// A search body as the app makes them, for listings of that status.
QJsonObject searchBody(ListingStatus status = ListingStatus::Available)
{
    const TradeData data{StatIndex::fromJson(tradefixtures::stats()),
                         TradeCategories::fromJson(tradefixtures::filters())};
    CustomSearchOptions options;
    options.status = status;
    options.itemClass = u"Helmets"_s;
    return customSearch({{.key = u"+# to maximum Life"_s, .min = 60}}, data, options).body;
}

// The listing status a search went out with.
QString sentStatus(const FakeNetwork::Sent &sent)
{
    const QJsonObject query = QJsonDocument::fromJson(sent.body).object().value(u"query").toObject();
    return query.value(u"status").toObject().value(u"option").toString();
}

TradeSettings inLeague(const QString &name, bool hardcore = false, bool soloSelfFound = false)
{
    TradeSettings settings;
    settings.league = {.name = name, .hardcore = hardcore, .soloSelfFound = soloSelfFound};
    return settings;
}

const QStringList siteLeagues = {u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s, u"Hardcore"_s};

// x1, x2...
QStringList listingIds(int count)
{
    QStringList ids;
    for (int i = 1; i <= count; ++i)
        ids << u"x%1"_s.arg(i);
    return ids;
}

// Shaped like the site's answer to a fetch, with only the ids in them, and
// null for those sold since the search.
QJsonObject listings(const QStringList &ids, const QStringList &sold = {})
{
    QJsonArray result;
    for (const QString &id : ids)
        result << (sold.contains(id) ? QJsonValue(QJsonValue::Null) : QJsonObject{{u"id"_s, id}});
    return {{u"result"_s, result}};
}

QList<QUrl> urls(const FakeNetwork &network)
{
    QList<QUrl> urls;
    for (const FakeNetwork::Sent &sent : network.sent)
        urls << sent.request.url();
    return urls;
}

// Waits for it to finish, if it hasn't.
template<typename Job>
bool finishes(Job *job)
{
    QSignalSpy finished(job, &Job::finished);
    return job->isFinished() || finished.wait();
}

} // namespace

class TestTradeSearcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what it says when the site lets it down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.trade.warning=false\n"
                                         u"pathofupgrading.trade.info=false"_s);
    }

    void searches()
    {
        const QJsonObject answer{
            {u"id"_s, u"Ab3dE9"_s},
            {u"total"_s, 250},
            // and something that isn't an id
            {u"result"_s, QJsonArray{u"x1"_s, u"x2"_s, QJsonValue::Null, u"x3"_s}},
        };
        FakeNetwork network;
        network.answers = {{.body = json(answer)}};
        TradeClient client(&network);
        TradeSearcher searcher(&client);
        searcher.setLeagues(siteLeagues);

        TradeSearch *search = searcher.search(searchBody());
        QVERIFY(search->url().isEmpty());
        QVERIFY(finishes(search));
        QCOMPARE(search->error(), TradeSearch::Error::None);
        QCOMPARE(search->id(), u"Ab3dE9"_s);
        QCOMPARE(search->total(), 250);
        QCOMPARE(search->listingIds(), (QStringList{u"x1"_s, u"x2"_s, u"x3"_s}));
        QCOMPARE(search->league(), u"Runes of Aldur"_s);
        QCOMPARE(search->url(), tradeResultsUrl(u"Runes of Aldur"_s, u"Ab3dE9"_s));

        QCOMPARE(urls(network), QList<QUrl>{tradeSearchUrl(u"Runes of Aldur"_s)});
        QCOMPARE(QJsonDocument::fromJson(network.sent.first().body).object(), searchBody());

        // and lets go of the reply
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(client.findChildren<TradeReply *>().isEmpty());
    }

    void followsTheLeague_data()
    {
        QTest::addColumn<TradeSettings>("settings");
        QTest::addColumn<QStringList>("leagues");
        QTest::addColumn<QString>("league");
        QTest::newRow("the current one") << TradeSettings() << siteLeagues << u"Runes of Aldur"_s;
        QTest::newRow("hardcore") << inLeague({}, true) << siteLeagues << u"HC Runes of Aldur"_s;
        QTest::newRow("Standard") << inLeague(u"Standard"_s) << siteLeagues << u"Standard"_s;
        QTest::newRow("hardcore Standard") << inLeague(u"Standard"_s, true) << siteLeagues << u"Hardcore"_s;
        QTest::newRow("one that ended")
            << inLeague(u"Forbidden Rites"_s, true) << siteLeagues << u"HC Runes of Aldur"_s;
        QTest::newRow("offline") << TradeSettings() << QStringList() << u"Standard"_s;
    }

    void followsTheLeague()
    {
        QFETCH(TradeSettings, settings);
        QFETCH(QStringList, leagues);
        QFETCH(QString, league);
        FakeNetwork network;
        TradeClient client(&network);
        TradeSearcher searcher(&client);
        searcher.setSettings(settings);
        searcher.setLeagues(leagues);

        QCOMPARE(searcher.league().tradeId(), league);
        QVERIFY(searcher.unavailable().isEmpty());
        TradeSearch *search = searcher.search(searchBody());
        QCOMPARE(urls(network), QList<QUrl>{tradeSearchUrl(league)});
        QCOMPARE(search->league(), league);
    }

    void unavailable_data()
    {
        QTest::addColumn<TradeSettings>("settings");
        QTest::addColumn<QString>("why");
        TradeSettings off;
        off.enabled = false;
        QTest::newRow("turned off") << off << u"Trade searches are turned off in the settings."_s;
        const QString solo = u"There's no trading in solo self-found, so trade searches are off."_s;
        QTest::newRow("solo self-found") << inLeague({}, false, true) << solo;
        QTest::newRow("hardcore SSF") << inLeague(u"Standard"_s, true, true) << solo;
    }

    void unavailable()
    {
        QFETCH(TradeSettings, settings);
        QFETCH(QString, why);
        FakeNetwork network;
        TradeClient client(&network);
        TradeSearcher searcher(&client);
        searcher.setSettings(settings);
        searcher.setLeagues(siteLeagues);
        QCOMPARE(searcher.unavailable(), why);

        TradeSearch *search = searcher.search(searchBody());
        QSignalSpy finished(search, &TradeSearch::finished);
        QCOMPARE(finished.size(), 0);
        QVERIFY(finished.wait());
        QCOMPARE(search->error(), TradeSearch::Error::Unavailable);
        QCOMPARE(search->errorString(), why);
        QVERIFY(search->url().isEmpty());
        QVERIFY(network.sent.isEmpty());
    }

    void takesNewSettings()
    {
        FakeNetwork network;
        TradeClient client(&network);
        TradeSearcher searcher(&client);
        searcher.setLeagues(siteLeagues);
        TradeSettings settings;
        settings.enabled = false;
        searcher.setSettings(settings);
        QVERIFY(finishes(searcher.search(searchBody())));
        QVERIFY(network.sent.isEmpty());

        settings.enabled = true;
        settings.league.hardcore = true;
        searcher.setSettings(settings);
        QVERIFY(finishes(searcher.search(searchBody())));

        // and the site's leagues, when a new one starts
        searcher.setLeagues({u"Forbidden Rites"_s, u"HC Forbidden Rites"_s, u"Standard"_s, u"Hardcore"_s});
        QVERIFY(finishes(searcher.search(searchBody())));
        QCOMPARE(urls(network), (QList<QUrl>{tradeSearchUrl(u"HC Runes of Aldur"_s),
                                             tradeSearchUrl(u"HC Forbidden Rites"_s)}));
    }

    void fails_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<TradeSearch::Error>("error");
        QTest::addColumn<QString>("why");
        QTest::addColumn<bool>("inTheBrowser"); // whether it's worth trying there

        QTest::newRow("rate limited")
            << FakeAnswer{.status = 429, .headers = {{"Retry-After", "30"}}}
            << TradeSearch::Error::RateLimited
            << u"The trade site has had too many requests from you. Try again in 30 seconds."_s << false;
        QTest::newRow("too complex") << siteError(u"Query is too complex."_s)
                                     << TradeSearch::Error::TooComplex
                                     << u"The trade site says the search has too many filters."_s << false;
        QTest::newRow("turned down") << siteError(u"Invalid query"_s) << TradeSearch::Error::Failed
                                     << u"The trade site says: Invalid query"_s << true;
        QTest::newRow("unreachable") << FakeAnswer{.error = QNetworkReply::HostNotFoundError}
                                     << TradeSearch::Error::Failed
                                     << u"Couldn't reach the trade site: Host www.pathofexile.com not found"_s
                                     << true;
        QTest::newRow("blocked") << FakeAnswer{.status = 403, .body = "<!DOCTYPE html>"}
                                 << TradeSearch::Error::Failed
                                 << u"The trade site's answer couldn't be read (HTTP 403)."_s << true;
        QTest::newRow("no search") << FakeAnswer{.body = json({{u"total"_s, 3}})}
                                   << TradeSearch::Error::Failed
                                   << u"The trade site's answer couldn't be read."_s << true;
    }

    void fails()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(TradeSearch::Error, error);
        QFETCH(QString, why);
        QFETCH(bool, inTheBrowser);
        FakeNetwork network;
        network.answers = {answer};
        TradeClient client(&network);
        TradeSearcher searcher(&client);

        TradeSearch *search = searcher.search(searchBody());
        QVERIFY(finishes(search));
        QCOMPARE(search->error(), error);
        QCOMPARE(search->errorString(), why);
        QCOMPARE(search->retryAfter(), error == TradeSearch::Error::RateLimited ? 30s : 0s);
        QCOMPARE(search->url(), inTheBrowser ? tradePrefilledUrl(u"Standard"_s, searchBody()) : QUrl());
        QVERIFY(search->listingIds().isEmpty());
        QCOMPARE(network.sent.size(), 1);
    }

    void triesOnlineListings()
    {
        FakeNetwork network;
        network.answers = {siteError(u"Status not supported"_s), found(u"Ab3dE9"_s, 3, {u"x1"_s})};
        TradeClient client(&network);
        TradeSearcher searcher(&client);

        TradeSearch *search = searcher.search(searchBody());
        QVERIFY(finishes(search));
        QCOMPARE(search->error(), TradeSearch::Error::None);
        QCOMPARE(search->id(), u"Ab3dE9"_s);
        QCOMPARE(network.sent.size(), 2);
        QCOMPARE(sentStatus(network.sent[0]), u"available"_s);
        // and the rest as it was
        QCOMPARE(QJsonDocument::fromJson(network.sent[1].body).object(), searchBody(ListingStatus::Online));

        // from then on
        QVERIFY(finishes(searcher.search(searchBody())));
        QCOMPARE(sentStatus(network.sent[2]), u"online"_s);
        // but only for that status
        QVERIFY(finishes(searcher.search(searchBody(ListingStatus::Securable))));
        QCOMPARE(sentStatus(network.sent[3]), u"securable"_s);
    }

    void givesUpOnTheStatus_data()
    {
        QTest::addColumn<QJsonObject>("body");
        QTest::addColumn<int>("sent");
        QJsonObject noStatus = searchBody();
        QJsonObject query = noStatus.value(u"query").toObject();
        query.remove(u"status"_s);
        noStatus.insert(u"query"_s, query);
        QTest::newRow("online already") << searchBody(ListingStatus::Online) << 1;
        QTest::newRow("without one") << noStatus << 1;
        QTest::newRow("online too") << searchBody() << 2;
    }

    void givesUpOnTheStatus()
    {
        QFETCH(QJsonObject, body);
        QFETCH(int, sent);
        FakeNetwork network;
        network.answers = {siteError(u"Invalid status option"_s), siteError(u"Invalid status option"_s)};
        TradeClient client(&network);
        TradeSearcher searcher(&client);

        TradeSearch *search = searcher.search(body);
        QVERIFY(finishes(search));
        QCOMPARE(search->error(), TradeSearch::Error::Failed);
        QCOMPARE(search->errorString(), u"The trade site says: Invalid status option"_s);
        QCOMPARE(network.sent.size(), sent);
        // the browser gets the last one that went
        QCOMPARE(
            search->url(),
            tradePrefilledUrl(u"Standard"_s, QJsonDocument::fromJson(network.sent.last().body).object()));
    }

    void fetches()
    {
        const QStringList ids = listingIds(25);
        const QStringList sold = {u"x14"_s};
        FakeNetwork network;
        network.answers = {{.body = json(listings(ids.first(10)))},
                           {.body = json(listings(ids.mid(10, 10), sold))},
                           {.body = json(listings(ids.sliced(20)))}};
        TradeClient client(&network);
        TradeSearcher searcher(&client);

        TradeFetch *fetch = searcher.fetch(ids, u"Ab3dE9"_s);
        QVERIFY(finishes(fetch));
        QCOMPARE(fetch->error(), TradeReply::Error::None);
        QCOMPARE(fetch->fetched(), 25);
        QCOMPARE(fetch->answer(), listings(ids, sold));
        QCOMPARE(urls(network), (QList<QUrl>{tradeFetchUrl(ids.first(10), u"Ab3dE9"_s),
                                             tradeFetchUrl(ids.mid(10, 10), u"Ab3dE9"_s),
                                             tradeFetchUrl(ids.sliced(20), u"Ab3dE9"_s)}));

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(client.findChildren<TradeReply *>().isEmpty());
    }

    void stopsFetchingAtAFailure()
    {
        const QStringList ids = listingIds(25);
        FakeNetwork network;
        network.answers = {{.body = json(listings(ids.first(10)))},
                           {.status = 429, .headers = {{"Retry-After", "5"}}}};
        TradeClient client(&network);
        TradeSearcher searcher(&client);

        TradeFetch *fetch = searcher.fetch(ids, u"Ab3dE9"_s);
        QVERIFY(finishes(fetch));
        QCOMPARE(fetch->error(), TradeReply::Error::RateLimited);
        QCOMPARE(fetch->retryAfter(), 5s);
        QCOMPARE(fetch->errorString(),
                 u"The trade site has had too many requests from you. Try again in 5 seconds."_s);
        // with what came before it
        QCOMPARE(fetch->fetched(), 10);
        QCOMPARE(fetch->answer(), listings(ids.first(10)));
        QCOMPARE(network.sent.size(), 2);
    }

    void fetchesNothing()
    {
        FakeNetwork network;
        TradeClient client(&network);
        TradeSearcher searcher(&client);

        TradeFetch *fetch = searcher.fetch({}, u"Ab3dE9"_s);
        QSignalSpy finished(fetch, &TradeFetch::finished);
        QCOMPARE(finished.size(), 0);
        QVERIFY(finished.wait());
        QCOMPARE(fetch->error(), TradeReply::Error::None);
        QCOMPARE(fetch->fetched(), 0);
        QCOMPARE(fetch->answer(), listings({}));
        QVERIFY(network.sent.isEmpty());
    }

    void callingOff()
    {
        FakeNetwork network;
        TradeClient client(&network);
        TradeSearcher searcher(&client);
        TradeSearch *search = searcher.search(searchBody());
        TradeFetch *fetch = searcher.fetch({u"x1"_s}, u"Ab3dE9"_s);
        QCOMPARE(network.sent.size(), 2);

        delete search;
        delete fetch;
        for (const FakeNetwork::Sent &sent : std::as_const(network.sent))
            QVERIFY(!sent.reply);
        QVERIFY(client.findChildren<TradeReply *>().isEmpty());
    }

    void deletingWhenFinished()
    {
        FakeNetwork network;
        TradeClient client(&network);
        TradeSearcher searcher(&client);
        TradeSearch *search = searcher.search(searchBody());
        TradeFetch *fetch = searcher.fetch({u"x1"_s}, u"Ab3dE9"_s);

        int finished = 0;
        connect(search, &TradeSearch::finished, this, [&finished, search] {
            finished += search->isFinished();
            delete search;
        });
        connect(fetch, &TradeFetch::finished, this, [&finished, fetch] {
            finished += fetch->isFinished();
            delete fetch;
        });
        QTRY_COMPARE(finished, 2);
    }
};

QTEST_GUILESS_MAIN(TestTradeSearcher)
#include "tst_tradesearcher.moc"
