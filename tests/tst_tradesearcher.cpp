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

// A search body as the app makes them.
QJsonObject searchBody()
{
    const TradeData data{StatIndex::fromJson(tradefixtures::stats()),
                         TradeCategories::fromJson(tradefixtures::filters())};
    CustomSearchOptions options;
    options.itemClass = u"Helmets"_s;
    return customSearch({{.key = u"+# to maximum Life"_s, .min = 60}}, data, options).body;
}

TradeSettings inLeague(const QString &name, bool hardcore = false, bool soloSelfFound = false)
{
    TradeSettings settings;
    settings.league = {.name = name, .hardcore = hardcore, .soloSelfFound = soloSelfFound};
    return settings;
}

const QStringList siteLeagues = {u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s, u"Hardcore"_s};

QList<QUrl> urls(const FakeNetwork &network)
{
    QList<QUrl> urls;
    for (const FakeNetwork::Sent &sent : network.sent)
        urls << sent.request.url();
    return urls;
}

// Waits for it to finish, if it hasn't.
bool finishes(TradeSearch *search)
{
    QSignalSpy finished(search, &TradeSearch::finished);
    return search->isFinished() || finished.wait();
}

} // namespace

class TestTradeSearcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what it says when the site lets it down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.trade.warning=false"_s);
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

        QTest::newRow("rate limited")
            << FakeAnswer{.status = 429, .headers = {{"Retry-After", "30"}}}
            << TradeSearch::Error::RateLimited
            << u"The trade site has had too many requests from you. Try again in 30 seconds."_s;
        QTest::newRow("too complex") << siteError(u"Query is too complex."_s)
                                     << TradeSearch::Error::TooComplex
                                     << u"The trade site says the search has too many filters."_s;
        QTest::newRow("turned down") << siteError(u"Invalid query"_s) << TradeSearch::Error::Failed
                                     << u"The trade site says: Invalid query"_s;
        QTest::newRow("unreachable")
            << FakeAnswer{.error = QNetworkReply::HostNotFoundError} << TradeSearch::Error::Failed
            << u"Couldn't reach the trade site: Host www.pathofexile.com not found"_s;
        QTest::newRow("blocked") << FakeAnswer{.status = 403, .body = "<!DOCTYPE html>"}
                                 << TradeSearch::Error::Failed
                                 << u"The trade site's answer couldn't be read (HTTP 403)."_s;
        QTest::newRow("no search") << FakeAnswer{.body = json({{u"total"_s, 3}})}
                                   << TradeSearch::Error::Failed
                                   << u"The trade site's answer couldn't be read."_s;
    }

    void fails()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(TradeSearch::Error, error);
        QFETCH(QString, why);
        FakeNetwork network;
        network.answers = {answer};
        TradeClient client(&network);
        TradeSearcher searcher(&client);

        TradeSearch *search = searcher.search(searchBody());
        QVERIFY(finishes(search));
        QCOMPARE(search->error(), error);
        QCOMPARE(search->errorString(), why);
        QCOMPARE(search->retryAfter(), error == TradeSearch::Error::RateLimited ? 30s : 0s);
        QVERIFY(search->url().isEmpty());
        QVERIFY(search->listingIds().isEmpty());
        QCOMPARE(network.sent.size(), 1);
    }

    void callingOff()
    {
        FakeNetwork network;
        TradeClient client(&network);
        TradeSearcher searcher(&client);
        TradeSearch *search = searcher.search(searchBody());
        QCOMPARE(network.sent.size(), 1);

        delete search;
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

        int finished = 0;
        connect(search, &TradeSearch::finished, this, [&finished, search] {
            finished += search->isFinished();
            delete search;
        });
        QTRY_COMPARE(finished, 1);
    }
};

QTEST_GUILESS_MAIN(TestTradeSearcher)
#include "tst_tradesearcher.moc"
