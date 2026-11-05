// SPDX-License-Identifier: MIT
#include "fakenetwork.h"
#include "services/ratesrefresher.h"
#include "services/ratesstore.h"

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

const QString league = u"Runes of Aldur"_s;

// What poe.ninja says, with a Divine Orb at 400 Exalted Orbs.
QByteArray overview()
{
    const QJsonObject core{{u"primary"_s, u"divine"_s}, {u"rates"_s, QJsonObject{{u"exalted"_s, 400}}}};
    const QJsonArray lines{QJsonObject{{u"id"_s, u"divine"_s}, {u"primaryValue"_s, 1}},
                           QJsonObject{{u"id"_s, u"exalted"_s}, {u"primaryValue"_s, 1 / 400.0}}};
    return QJsonDocument(QJsonObject{{u"core"_s, core}, {u"lines"_s, lines}}).toJson(QJsonDocument::Compact);
}

const FakeAnswer good{.body = overview()};
const FakeAnswer down{.error = QNetworkReply::HostNotFoundError};

// With a minute that lasts 10 ms, so the waits are short.
struct Refresher
{
    QTemporaryDir dir;
    FakeNetwork network;
    RatesStore store{&network, dir.path()};
    RatesRefresher refresher{&store};

    Refresher() { refresher.setMinute(10ms); }

    // Whether the store gets an answer, which the refresher has heard of
    // first, as it was connected first.
    bool answers()
    {
        QSignalSpy refreshed(&store, &RatesStore::refreshed);
        return refreshed.wait(10000);
    }
};

} // namespace

class TestRatesRefresher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what it says when poe.ninja lets it down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.ninja.warning=false"_s);
    }

    void asksAgainEachTime()
    {
        Refresher r;
        r.network.answers = {good, good, good};
        r.refresher.follow(league, 2);
        QVERIFY(r.refresher.isRunning());
        QCOMPARE(r.network.sent.size(), 0);

        // even though what it has is fresh
        QTRY_VERIFY_WITH_TIMEOUT(r.network.sent.size() >= 3, 5000);
        QCOMPARE(r.network.sent.at(0).request.url(), ninjaCurrencyUrl(league));
        QCOMPARE(r.store.league(), league);
        QCOMPARE(r.store.divineRate(), 400.0);
    }

    void doesNothingUntilTheWaitIsOver()
    {
        Refresher r;
        r.refresher.setMinute(1h);
        r.refresher.follow(league, 10);
        QTest::qWait(100);
        QCOMPARE(r.network.sent.size(), 0);
    }

    void stops()
    {
        Refresher r;
        r.refresher.follow(league, 1);
        r.refresher.stop();
        QVERIFY(!r.refresher.isRunning());
        QTest::qWait(100);
        QCOMPARE(r.network.sent.size(), 0);

        // as does having no league
        r.refresher.follow(league, 1);
        r.refresher.follow(QString(), 1);
        QVERIFY(!r.refresher.isRunning());
        QTest::qWait(100);
        QCOMPARE(r.network.sent.size(), 0);
    }

    void backsOffAndRecovers()
    {
        Refresher r;
        r.refresher.setMinute(5ms);
        r.network.answers = {down, down, down, down, good};
        r.refresher.follow(league, 10);
        QCOMPARE(r.refresher.waitMinutes(), 10);

        // The wait after each answer, taken as it comes in. A slow machine
        // can get several in before the test looks.
        QList<int> waits;
        QList<bool> succeeded;
        connect(&r.store, &RatesStore::refreshed, &r.store, [&] {
            waits.append(r.refresher.waitMinutes());
            succeeded.append(r.store.errorString().isEmpty());
        });
        QTRY_VERIFY_WITH_TIMEOUT(waits.size() >= 5, 10000);

        // twice as long each time, up to an hour, and back to what it was
        // when poe.ninja answers
        QCOMPARE(waits.first(5), (QList<int>{20, 40, 60, 60, 10}));
        QCOMPARE(succeeded.first(5), (QList<bool>{false, false, false, false, true}));
    }

    void followingTheSameThingKeepsTheBackoff()
    {
        Refresher r;
        // the second ask never gets an answer, so the wait stays put
        r.network.answers = {down, {.never = true}};
        r.refresher.follow(league, 3);
        QVERIFY(r.answers());
        QCOMPARE(r.refresher.waitMinutes(), 6);

        r.refresher.follow(league, 3);
        QCOMPARE(r.refresher.waitMinutes(), 6);
        // but another wait starts again
        r.refresher.follow(league, 4);
        QCOMPARE(r.refresher.waitMinutes(), 4);
    }

    void waitsAreHeldToTheLimits()
    {
        Refresher r;
        r.refresher.setMinute(1h);
        r.refresher.follow(league, 500);
        QCOMPARE(r.refresher.waitMinutes(), 60);
        r.refresher.follow(league, 0);
        QCOMPARE(r.refresher.waitMinutes(), 1);
    }

    // a search's refresh is the store's own business
    void doesNotAskWhileOneIsUnderWay()
    {
        Refresher r;
        r.network.answers = {{.never = true}, good};
        r.store.refresh(league);
        QVERIFY(r.store.isRefreshing());
        QCOMPARE(r.network.sent.size(), 1);

        r.refresher.follow(league, 1);
        QTest::qWait(100);
        QCOMPARE(r.network.sent.size(), 1);
        QVERIFY(r.refresher.isRunning());

        // and when that one lets poe.ninja down, it's not the refresher's
        // failure to back off for
        QSignalSpy refreshed(&r.store, &RatesStore::refreshed);
        Q_EMIT r.network.sent.at(0).reply->finished();
        QCOMPARE(refreshed.size(), 1);
        QVERIFY(!r.store.errorString().isEmpty());
        QCOMPARE(r.refresher.waitMinutes(), 1);

        // it goes on to ask itself
        QTRY_COMPARE_WITH_TIMEOUT(r.network.sent.size(), 2, 5000);
    }
};

QTEST_GUILESS_MAIN(TestRatesRefresher)
#include "tst_ratesrefresher.moc"
