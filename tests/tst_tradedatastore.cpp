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
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
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

// The files the site's answers were kept in.
bool keepAll(const QString &folder, const QString &league)
{
    return writeJsonFile(folder + u"/stats.json"_s, tradefixtures::stats()) &&
           writeJsonFile(folder + u"/filters.json"_s, tradefixtures::filters()) &&
           writeJsonFile(folder + u"/leagues.json"_s, leagues({league}));
}

bool refreshes(TradeDataStore &store)
{
    QSignalSpy refreshed(&store, &TradeDataStore::refreshed);
    store.refresh();
    return refreshed.wait();
}

} // namespace

class TestTradeDataStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
        QVERIFY(keepAll(dir.path(), u"Runes of Aldur"_s));
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

    void oneRefreshAtATime()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        TradeClient client(&network);
        TradeDataStore store(&client, dir.path());
        QSignalSpy refreshed(&store, &TradeDataStore::refreshed);

        store.refresh();
        QVERIFY(store.isRefreshing());
        store.refresh();
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
