// SPDX-License-Identifier: MIT
#include "core/datafiles.h"
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "services/tradeclient.h"
#include "services/tradedatastore.h"
#include "tradefixtures.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

namespace {

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// Shaped like /api/trade2/data/leagues
QJsonObject leagues(const QStringList &ids)
{
    QJsonArray result;
    for (const QString &id : ids)
        result << QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}};
    return {{u"result"_s, result}};
}

// Stats with only the one in them, to tell them from the fixture's.
QJsonObject oneStat(const QString &id)
{
    using namespace tradefixtures;
    return response({group(u"explicit"_s, {stat(id, u"+# to Spirit"_s, u"explicit"_s)})});
}

QList<FakeAnswer> answers(const QJsonObject &stats, const QJsonObject &filters, const QJsonObject &leagues)
{
    return {{.body = json(stats)}, {.body = json(filters)}, {.body = json(leagues)}};
}

QList<QUrl> urls(const FakeNetwork &network)
{
    QList<QUrl> urls;
    for (const FakeNetwork::Sent &sent : network.sent)
        urls << sent.request.url();
    return urls;
}

const QList<QUrl> everything = {tradeDataUrl(u"stats"_s), tradeDataUrl(u"filters"_s),
                                tradeDataUrl(u"leagues"_s)};

// A file the site's answer was kept in that long ago.
bool keep(const QString &folder, const QString &name, const QJsonObject &answer, std::chrono::hours age)
{
    const QString path = folder + u'/' + name + u".json"_s;
    QFile file(path);
    return writeJsonFile(path, answer) && file.open(QIODevice::ReadWrite) &&
           file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-std::chrono::seconds(age).count()),
                            QFileDevice::FileModificationTime);
}

bool keepAll(const QString &folder, std::chrono::hours age, const QString &league = u"Standard"_s)
{
    return keep(folder, u"stats"_s, tradefixtures::stats(), age) &&
           keep(folder, u"filters"_s, tradefixtures::filters(), age) &&
           keep(folder, u"leagues"_s, leagues({league}), age);
}

bool refreshes(TradeDataStore &store, bool force = false)
{
    QSignalSpy refreshed(&store, &TradeDataStore::refreshed);
    store.refresh(force);
    return refreshed.wait();
}

} // namespace

class TestTradeDataStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what it says when the site lets it down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.trade.warning=false"_s);
    }

    void asksForWhatsMissing()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        network.answers = answers(tradefixtures::stats(), tradefixtures::filters(),
                                  leagues({u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s}));
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());

        QVERIFY(refreshes(store));
        QCOMPARE(urls(network), everything);
        QCOMPARE(store.data().stats.all().size(), 7);
        QCOMPARE(store.data().categories.categoryFor(u"Quarterstaves"_s), u"weapon.warstaff"_s);
        QCOMPARE(store.leagues(), (QStringList{u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s}));
        QVERIFY(store.errorString().isEmpty());

        // and keeps it
        for (const QString &name : {u"stats"_s, u"filters"_s, u"leagues"_s})
            QVERIFY(readJsonFile(dir.filePath(name + u".json"_s)));
        TradeDataStore again(&client, dir.path());
        QVERIFY(refreshes(again));
        QCOMPARE(network.sent.size(), 3);
        QCOMPARE(again.data().stats.all().size(), 7);
        QCOMPARE(again.leagues(), store.leagues());

        // and lets go of the replies
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(client.findChildren<TradeReply *>().isEmpty());
    }

    void readsTheFiles()
    {
        QTemporaryDir dir;
        QVERIFY(keepAll(dir.path(), 23h, u"Runes of Aldur"_s));
        FakeNetwork network;
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());

        QSignalSpy refreshed(&store, &TradeDataStore::refreshed);
        store.refresh();
        QCOMPARE(refreshed.size(), 0);
        QVERIFY(refreshed.wait());
        QVERIFY(network.sent.isEmpty());
        QVERIFY(!store.isRefreshing());
        QCOMPARE(store.data().stats.all().size(), 7);
        QVERIFY(!store.data().categories.isEmpty());
        QCOMPARE(store.leagues(), QStringList{u"Runes of Aldur"_s});
    }

    void onlyReads()
    {
        QTemporaryDir dir;
        // which refresh() would ask for again
        QVERIFY(keepAll(dir.path(), 25h, u"Runes of Aldur"_s));
        FakeNetwork network;
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());

        QSignalSpy refreshed(&store, &TradeDataStore::refreshed);
        store.load();
        QCOMPARE(store.data().stats.all().size(), 7);
        QCOMPARE(store.leagues(), QStringList{u"Runes of Aldur"_s});
        QVERIFY(!store.isRefreshing());
        QCoreApplication::processEvents();
        QVERIFY(network.sent.isEmpty());
        QCOMPARE(refreshed.size(), 0);

        // and only once
        for (const QString &name : {u"stats"_s, u"filters"_s, u"leagues"_s})
            QVERIFY(QFile::remove(dir.filePath(name + u".json"_s)));
        QVERIFY(keep(dir.path(), u"stats"_s, oneStat(u"explicit.stat_spirit"_s), 1h));
        store.load();
        QCOMPARE(store.data().stats.all().size(), 7);
    }

    void readsPastBadFiles_data()
    {
        QTest::addColumn<QByteArray>("file");
        QTest::newRow("not JSON") << QByteArray("{\"result\": [");
        QTest::newRow("no stats") << QByteArray("{}");
    }

    void readsPastBadFiles()
    {
        QFETCH(QByteArray, file);
        QTemporaryDir dir;
        QVERIFY(keepAll(dir.path(), 1h));
        QVERIFY(writeFile(dir.filePath(u"stats.json"_s), file));
        FakeNetwork network;
        network.answers = {{.body = json(tradefixtures::stats())}};
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());

        QVERIFY(refreshes(store));
        QCOMPARE(urls(network), QList<QUrl>{tradeDataUrl(u"stats"_s)});
        QCOMPARE(store.data().stats.all().size(), 7);
    }

    void asksAgain_data()
    {
        QTest::addColumn<std::chrono::hours>("age");
        QTest::addColumn<bool>("force");
        QTest::newRow("a day old") << 25h << false;
        QTest::newRow("from the future") << -2h << false;
        QTest::newRow("forced") << 1h << true;
    }

    void asksAgain()
    {
        QFETCH(std::chrono::hours, age);
        QFETCH(bool, force);
        QTemporaryDir dir;
        QVERIFY(keepAll(dir.path(), age));
        FakeNetwork network;
        network.answers = answers(oneStat(u"explicit.stat_spirit"_s), tradefixtures::filters(),
                                  leagues({u"Forbidden Rites"_s}));
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());

        QVERIFY(refreshes(store, force));
        QCOMPARE(urls(network), everything);
        QCOMPARE(store.data().stats.all().size(), 1);
        QCOMPARE(store.leagues(), QStringList{u"Forbidden Rites"_s});
        QCOMPARE(readJsonFile(dir.filePath(u"leagues.json"_s)).value_or(QJsonObject()),
                 leagues({u"Forbidden Rites"_s}));

        // and not again today
        QVERIFY(refreshes(store));
        QCOMPARE(network.sent.size(), 3);
    }

    void keepsWhatItHas_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::newRow("unreachable") << FakeAnswer{.error = QNetworkReply::HostNotFoundError};
        QTest::newRow("turned down") << FakeAnswer{.status = 503, .body = "<html>"};
        QTest::newRow("empty") << FakeAnswer{.body = json({{u"result"_s, QJsonArray()}})};
    }

    void keepsWhatItHas()
    {
        QFETCH(FakeAnswer, answer);
        QTemporaryDir dir;
        QVERIFY(keepAll(dir.path(), 48h, u"Runes of Aldur"_s));
        FakeNetwork network;
        network.answers = {answer, answer, answer};
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());

        QVERIFY(refreshes(store));
        QCOMPARE(network.sent.size(), 3);
        QCOMPARE(store.data().stats.all().size(), 7);
        QVERIFY(!store.data().categories.isEmpty());
        QCOMPARE(store.leagues(), QStringList{u"Runes of Aldur"_s});
        QVERIFY(store.errorString().isEmpty());
        QCOMPARE(readJsonFile(dir.filePath(u"stats.json"_s)).value_or(QJsonObject()), tradefixtures::stats());
    }

    void saysWhyThereAreNoStats_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<QString>("why");
        QTest::newRow("unreachable") << FakeAnswer{.error = QNetworkReply::HostNotFoundError}
                                     << u"Searches need the trade site's list of stats. Couldn't reach the "
                                        u"trade site: Host www.pathofexile.com not found"_s;
        QTest::newRow("empty")
            << FakeAnswer{.body = json({{u"result"_s, QJsonArray()}})}
            << u"Searches need the trade site's list of stats, and it sent an empty one."_s;
    }

    void saysWhyThereAreNoStats()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(QString, why);
        QTemporaryDir dir;
        FakeNetwork network;
        network.answers = {
            answer, {.body = json(tradefixtures::filters())}, {.body = json(leagues({u"Standard"_s}))}};
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());

        QVERIFY(refreshes(store));
        QVERIFY(store.data().stats.isEmpty());
        QCOMPARE(store.errorString(), why);
        QVERIFY(!QFile::exists(dir.filePath(u"stats.json"_s)));
        QCOMPARE(store.leagues(), QStringList{u"Standard"_s});

        // until it has them
        network.answers = {{.body = json(tradefixtures::stats())}};
        QVERIFY(refreshes(store));
        QCOMPARE(network.sent.size(), 4);
        QVERIFY(!store.data().stats.isEmpty());
        QVERIFY(store.errorString().isEmpty());
    }

    void blamesOnlyTheStats()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        network.answers = {{.error = QNetworkReply::HostNotFoundError},
                           {.body = json({{u"result"_s, QJsonArray()}})},
                           {.status = 503}};
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());

        QVERIFY(refreshes(store));
        QCOMPARE(store.errorString(), u"Searches need the trade site's list of stats. Couldn't reach the "
                                      u"trade site: Host www.pathofexile.com not found"_s);
    }

    void oneRefreshAtATime()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());
        QSignalSpy refreshed(&store, &TradeDataStore::refreshed);

        store.refresh();
        QVERIFY(store.isRefreshing());
        store.refresh(true);
        QVERIFY(refreshed.wait());
        QCOMPARE(network.sent.size(), 3);
        QVERIFY(!store.isRefreshing());
        QTest::qWait(10);
        QCOMPARE(refreshed.size(), 1);
    }

    void callingOff()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        TradeClient client(&network);
        auto *store = new TradeDataStore(&client, dir.path());
        store->refresh();
        QCOMPARE(network.sent.size(), 3);
        delete store;
        for (const FakeNetwork::Sent &sent : std::as_const(network.sent))
            QVERIFY(!sent.reply);
        QTest::qWait(10);
        QVERIFY(!QFile::exists(dir.filePath(u"stats.json"_s)));
    }
};

QTEST_GUILESS_MAIN(TestTradeDataStore)
#include "tst_tradedatastore.moc"
