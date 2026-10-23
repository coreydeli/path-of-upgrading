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
