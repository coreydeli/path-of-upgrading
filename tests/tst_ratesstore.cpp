// SPDX-License-Identifier: MIT
#include "core/datafiles.h"
#include "core/rates.h"
#include "fakenetwork.h"
#include "services/ratesstore.h"

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

const QString softcore = u"Runes of Aldur"_s;
const QString hardcore = u"HC Runes of Aldur"_s;

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject line(const QString &id, double primaryValue)
{
    return {{u"id"_s, id}, {u"primaryValue"_s, primaryValue}};
}

// Shaped like poe.ninja's currency overview, which prices everything in
// Divine Orbs.
QJsonObject overview(double exalts, double chaos = 10)
{
    const QJsonObject core{{u"primary"_s, u"divine"_s},
                           {u"secondary"_s, u"chaos"_s},
                           {u"rates"_s, QJsonObject{{u"exalted"_s, exalts}, {u"chaos"_s, chaos}}}};
    const QJsonArray lines{line(u"divine"_s, 1), line(u"exalted"_s, 1 / exalts), line(u"chaos"_s, 1 / chaos)};
    return {{u"core"_s, core}, {u"lines"_s, lines}};
}

// What poe.ninja says about a league it doesn't know.
const QByteArray unknownLeague =
    R"({"core":{"items":[],"rates":{},"primary":"chaos","secondary":"divine"},"lines":[],"items":[]})";

QString fileOf(const QString &folder, const QString &league)
{
    return folder + u'/' + league + u".json"_s;
}

// A file an overview was kept in that long ago.
bool keep(const QString &path, const QJsonObject &overview, std::chrono::minutes age)
{
    QFile file(path);
    return writeJsonFile(path, overview) && file.open(QIODevice::ReadWrite) &&
           file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-std::chrono::seconds(age).count()),
                            QFileDevice::FileModificationTime);
}

int minutesOld(const RatesStore &store)
{
    return int(store.fetched().secsTo(QDateTime::currentDateTimeUtc()) / 60);
}

bool refreshes(RatesStore &store, const QString &league, bool force = false)
{
    QSignalSpy refreshed(&store, &RatesStore::refreshed);
    store.refresh(league, force);
    return refreshed.wait();
}

const QString unreachable = u"Couldn't reach poe.ninja: Host poe.ninja not found"_s;

} // namespace

class TestRatesStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what it says when poe.ninja lets it down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.ninja.warning=false"_s);
    }

    void asksPoeNinja()
    {
        QTemporaryDir dir;
        const QString folder = dir.filePath(u"ninja"_s);
        FakeNetwork network;
        network.answers = {{.body = json(overview(400))}};
        RatesStore store(&network, folder);

        QVERIFY(refreshes(store, softcore));
        QCOMPARE(network.sent.size(), 1);
        QCOMPARE(network.sent.at(0).request.url(), ninjaCurrencyUrl(softcore));
        QCOMPARE(store.league(), softcore);
        QCOMPARE(store.divineRate(), 400.0);
        QCOMPARE(store.rates().value(u"chaos"_s), 40.0);
        QCOMPARE(store.rates().value(u"exalted"_s), 1.0);
        QCOMPARE(minutesOld(store), 0);
        QVERIFY(store.errorString().isEmpty());
        QVERIFY(!store.isRefreshing());

        // and keeps them
        QCOMPARE(readJsonFile(fileOf(folder, softcore)).value_or(QJsonObject()), overview(400));
        RatesStore again(&network, folder);
        QVERIFY(refreshes(again, softcore));
        QCOMPARE(network.sent.size(), 1);
        QCOMPARE(again.rates(), store.rates());

        // and lets go of the reply
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!network.sent.at(0).reply);
    }

    // poe.ninja's prices have four figures, which can make for more in a rate
    void roundsTheDivineRate()
    {
        QTemporaryDir dir;
        QJsonObject data = overview(539.6);
        data.insert(u"lines"_s, QJsonArray{line(u"divine"_s, 1), line(u"exalted"_s, 0.001853)});
        FakeNetwork network;
        network.answers = {{.body = json(data)}};
        RatesStore store(&network, dir.path());

        QVERIFY(refreshes(store, softcore));
        QCOMPARE(store.rates().value(u"divine"_s), 539.7);
    }

    void readsTheFile()
    {
        QTemporaryDir dir;
        QVERIFY(keep(fileOf(dir.path(), hardcore), overview(120, 5), 59min));
        FakeNetwork network;
        RatesStore store(&network, dir.path());

        QSignalSpy refreshed(&store, &RatesStore::refreshed);
        store.refresh(hardcore);
        QCOMPARE(refreshed.size(), 0);
        QVERIFY(refreshed.wait());
        QVERIFY(network.sent.isEmpty());
        QCOMPARE(store.divineRate(), 120.0);
        QCOMPARE(store.rates().value(u"chaos"_s), 24.0);
        QCOMPARE(minutesOld(store), 59);
    }

    void readsPastBadFiles_data()
    {
        QTest::addColumn<QByteArray>("file");
        QTest::newRow("not JSON") << QByteArray(R"({"core": [)");
        QTest::newRow("no Divine Orb price") << unknownLeague;
    }

    void readsPastBadFiles()
    {
        QFETCH(QByteArray, file);
        QTemporaryDir dir;
        QVERIFY(writeFile(fileOf(dir.path(), softcore), file));
        FakeNetwork network;
        network.answers = {{.body = json(overview(400))}};
        RatesStore store(&network, dir.path());

        QVERIFY(refreshes(store, softcore));
        QCOMPARE(network.sent.size(), 1);
        QCOMPARE(store.divineRate(), 400.0);
    }

    void asksAgain_data()
    {
        QTest::addColumn<std::chrono::minutes>("age");
        QTest::addColumn<bool>("force");
        QTest::newRow("an hour old") << 61min << false;
        QTest::newRow("from the future") << -10min << false;
        QTest::newRow("forced") << 10min << true;
    }

    void asksAgain()
    {
        QFETCH(std::chrono::minutes, age);
        QFETCH(bool, force);
        QTemporaryDir dir;
        QVERIFY(keep(fileOf(dir.path(), softcore), overview(400), age));
        FakeNetwork network;
        network.answers = {{.body = json(overview(450))}};
        RatesStore store(&network, dir.path());

        QVERIFY(refreshes(store, softcore, force));
        QCOMPARE(network.sent.size(), 1);
        QCOMPARE(store.divineRate(), 450.0);
        QCOMPARE(minutesOld(store), 0);
        QCOMPARE(readJsonFile(fileOf(dir.path(), softcore)).value_or(QJsonObject()), overview(450));

        // and not again for an hour
        QVERIFY(refreshes(store, softcore));
        QCOMPARE(network.sent.size(), 1);
    }

    void keepsWhatItHas_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<QString>("why");
        QTest::newRow("unreachable") << FakeAnswer{.error = QNetworkReply::HostNotFoundError} << unreachable;
        QTest::newRow("timed out") << FakeAnswer{.error = QNetworkReply::TimeoutError}
                                   << u"poe.ninja didn't answer in time."_s;
        // as older versions of Qt report it
        QTest::newRow("timed out, canceled") << FakeAnswer{.error = QNetworkReply::OperationCanceledError}
                                             << u"poe.ninja didn't answer in time."_s;
        QTest::newRow("turned down") << FakeAnswer{.status = 503,
                                                   .body = R"({"title":"Service Unavailable","status":503})"}
                                     << u"poe.ninja's answer couldn't be read (HTTP 503)."_s;
        QTest::newRow("not JSON") << FakeAnswer{.body = "<html>"}
                                  << u"poe.ninja's answer couldn't be read (HTTP 200)."_s;
        QTest::newRow("no Divine Orb price") << FakeAnswer{.body = unknownLeague}
                                             << u"poe.ninja has no Divine Orb price for Runes of Aldur."_s;
    }

    void keepsWhatItHas()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(QString, why);
        QTemporaryDir dir;
        QVERIFY(keep(fileOf(dir.path(), softcore), overview(400), 90min));
        FakeNetwork network;
        network.answers = {answer};
        RatesStore store(&network, dir.path());

        QVERIFY(refreshes(store, softcore));
        QCOMPARE(network.sent.size(), 1);
        QCOMPARE(store.errorString(), why);
        QCOMPARE(store.divineRate(), 400.0);
        QCOMPARE(minutesOld(store), 90);
        QCOMPARE(readJsonFile(fileOf(dir.path(), softcore)).value_or(QJsonObject()), overview(400));
    }

    void saysWhyThereAreNone()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        network.answers = {{.error = QNetworkReply::HostNotFoundError}};
        RatesStore store(&network, dir.path());

        QVERIFY(refreshes(store, softcore));
        QVERIFY(store.rates().isEmpty());
        QCOMPARE(store.divineRate(), 0.0);
        QVERIFY(!store.fetched().isValid());
        QCOMPARE(store.errorString(), unreachable);
        QVERIFY(!QFile::exists(fileOf(dir.path(), softcore)));
    }

    void holdsOffAfterAFailure()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        network.answers = {{.error = QNetworkReply::HostNotFoundError}};
        RatesStore store(&network, dir.path());
        QVERIFY(refreshes(store, softcore));

        QVERIFY(refreshes(store, softcore));
        QCOMPARE(network.sent.size(), 1);
        QCOMPARE(store.errorString(), unreachable);

        // unless forced
        network.answers = {{.body = json(overview(400))}};
        QVERIFY(refreshes(store, softcore, true));
        QCOMPARE(network.sent.size(), 2);
        QCOMPARE(store.divineRate(), 400.0);
        QVERIFY(store.errorString().isEmpty());
    }

    void oneRefreshAtATime()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        network.answers = {{.body = json(overview(400))}};
        RatesStore store(&network, dir.path());
        QSignalSpy refreshed(&store, &RatesStore::refreshed);

        store.refresh(softcore);
        QVERIFY(store.isRefreshing());
        store.refresh(softcore, true);
        QVERIFY(refreshed.wait());
        QCOMPARE(network.sent.size(), 1);
        QVERIFY(!store.isRefreshing());
        QTest::qWait(10);
        QCOMPARE(refreshed.size(), 1);
    }

    void startsOverForAnotherLeague()
    {
        QTemporaryDir dir;
        QVERIFY(keep(fileOf(dir.path(), hardcore), overview(120), 20min));
        FakeNetwork network;
        network.answers = {{.error = QNetworkReply::HostNotFoundError}, {.status = 503}};
        RatesStore store(&network, dir.path());
        QVERIFY(refreshes(store, softcore));

        QVERIFY(refreshes(store, hardcore));
        QCOMPARE(network.sent.size(), 1);
        QCOMPARE(store.league(), hardcore);
        QCOMPARE(store.divineRate(), 120.0);
        QCOMPARE(minutesOld(store), 20);
        QVERIFY(store.errorString().isEmpty());

        // and asks again for the one it couldn't get, with none of the other's
        QVERIFY(refreshes(store, softcore));
        QCOMPARE(network.sent.size(), 2);
        QVERIFY(store.rates().isEmpty());
        QVERIFY(!store.fetched().isValid());
        QCOMPARE(store.errorString(), u"poe.ninja's answer couldn't be read (HTTP 503)."_s);
    }

    void callsOffTheLastLeagues()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        network.answers = {{.body = json(overview(400))}, {.body = json(overview(120))}};
        RatesStore store(&network, dir.path());
        QSignalSpy refreshed(&store, &RatesStore::refreshed);

        store.refresh(softcore);
        store.refresh(hardcore);
        QCOMPARE(network.sent.size(), 2);
        QVERIFY(!network.sent.at(0).reply);
        QCOMPARE(network.sent.at(1).request.url(), ninjaCurrencyUrl(hardcore));
        QVERIFY(refreshed.wait());
        QCOMPARE(store.league(), hardcore);
        QCOMPARE(store.divineRate(), 120.0);
        QTest::qWait(10);
        QCOMPARE(refreshed.size(), 1);
        QVERIFY(!QFile::exists(fileOf(dir.path(), softcore)));
    }

    void fileNames()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        network.answers = {{.body = json(overview(400))}};
        RatesStore store(&network, dir.path());

        QVERIFY(refreshes(store, u"Ours/Theirs (PL12345)"_s));
        QVERIFY(QFile::exists(dir.filePath(u"Ours%2FTheirs %28PL12345%29.json"_s)));
    }

    void callingOff()
    {
        QTemporaryDir dir;
        FakeNetwork network;
        auto *store = new RatesStore(&network, dir.path());
        store->refresh(softcore);
        QCOMPARE(network.sent.size(), 1);
        delete store;
        QVERIFY(!network.sent.at(0).reply);
    }
};

QTEST_GUILESS_MAIN(TestRatesStore)
#include "tst_ratesstore.moc"
