// SPDX-License-Identifier: MIT
#include "fakenetwork.h"
#include "services/updatechecker.h"
#include "updatetexts.h"

#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

Q_DECLARE_METATYPE(FakeAnswer)

namespace {

Version v(const QString &text)
{
    return parseVersion(text).value_or(Version{});
}

FakeAnswer releases(const QList<QString> &versions)
{
    QList<QJsonObject> list;
    for (const QString &version : versions)
        list << fakeRelease(version, version.contains(u"beta"_s));
    return {.body = fakeReleases(list)};
}

// Waits for it to have asked and heard back.
bool settles(UpdateChecker &checker)
{
    return QTest::qWaitFor([&] { return !checker.isChecking(); }, 5000);
}

} // namespace

class TestUpdateChecker : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // what it says when GitHub lets it down, which the tests make it do
        QLoggingCategory::setFilterRules(u"pathofupgrading.updates.warning=false"_s);
    }

    void asksGitHubForTheReleasesAsThisProject()
    {
        FakeNetwork network;
        network.answers = {releases({u"0.7.0"_s})};
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        QSignalSpy found(&checker, &UpdateChecker::found);
        checker.check();
        QVERIFY(checker.isChecking());
        QCOMPARE(network.sent.size(), 1);
        const QNetworkRequest &request = network.sent.at(0).request;
        QCOMPARE(request.url().host(), u"api.github.com"_s);
        QCOMPARE(request.url().path(), u"/repos/coreydeli/path-of-upgrading/releases"_s);
        QCOMPARE(request.url().scheme(), u"https"_s);
        QVERIFY(
            request.header(QNetworkRequest::UserAgentHeader).toByteArray().startsWith("PathOfUpgrading/"));
        QVERIFY(settles(checker));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.at(0).at(0).value<Release>().tag, u"v0.7.0"_s);
        QCOMPARE(checker.newest()->tag, u"v0.7.0"_s);
        QVERIFY(checker.isOffered());
        QVERIFY(checker.checked().isValid());
        QVERIFY(checker.errorString().isEmpty());
    }

    void sayssNothingWhenUpToDate()
    {
        FakeNetwork network;
        network.answers = {releases({u"0.7.0"_s, u"0.6.0"_s})};
        UpdateChecker checker(&network, v(u"0.7.0"_s));
        QSignalSpy found(&checker, &UpdateChecker::found);
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY(!checker.newest());
        QVERIFY(!checker.isOffered());
        QCOMPARE(found.size(), 0);
        QVERIFY(checker.checked().isValid());
    }

    void startChecksUnlessTurnedOff()
    {
        FakeNetwork network;
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        checker.follow({.checkOnLaunch = false});
        checker.start();
        QCOMPARE(network.sent.size(), 0);
        QVERIFY(!checker.isChecking());

        // though asking it to is asking
        checker.check();
        QCOMPARE(network.sent.size(), 1);
        QVERIFY(settles(checker));
    }

    void startChecksOnce()
    {
        FakeNetwork network;
        network.answers = {releases({})};
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        checker.start();
        QCOMPARE(network.sent.size(), 1);
        // while it's still asking, it doesn't ask again
        checker.check();
        checker.check();
        QCOMPARE(network.sent.size(), 1);
        QVERIFY(settles(checker));
    }

    void asksAgainEachPeriod()
    {
        FakeNetwork network;
        network.answers = {releases({}), releases({}), releases({})};
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        checker.setPeriod(30ms);
        checker.start();
        QTRY_VERIFY_WITH_TIMEOUT(network.sent.size() >= 3, 5000);
    }

    void doesntAskEachPeriodWhenTurnedOff()
    {
        FakeNetwork network;
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        checker.follow({.checkOnLaunch = false});
        checker.setPeriod(10ms);
        checker.start();
        QTest::qWait(100);
        QCOMPARE(network.sent.size(), 0);

        // and turning it back on makes it ask on the next one
        checker.follow({.checkOnLaunch = true});
        QTRY_VERIFY_WITH_TIMEOUT(network.sent.size() >= 1, 5000);
    }

    void betasOnlyWhenIncluded()
    {
        FakeNetwork network;
        network.answers = {releases({u"0.7.0-beta.2"_s, u"0.6.0"_s})};
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        checker.follow({.includeBetas = false});
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY(!checker.newest());

        // from what it was told, without asking again
        QSignalSpy changed(&checker, &UpdateChecker::changed);
        checker.follow({.includeBetas = true});
        QCOMPARE(checker.newest()->tag, u"v0.7.0-beta.2"_s);
        QVERIFY(changed.size() >= 1);
        QCOMPARE(network.sent.size(), 1);
        checker.follow({.includeBetas = false});
        QVERIFY(!checker.newest());
    }

    void aSkippedVersionIsKeptButNotOffered()
    {
        FakeNetwork network;
        network.answers = {releases({u"0.7.0-beta.2"_s}), releases({u"0.7.0-beta.3"_s})};
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        QSignalSpy found(&checker, &UpdateChecker::found);
        checker.follow({.includeBetas = true, .skipped = u"0.7.0-beta.2"_s});
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY(checker.newest());
        QVERIFY(checker.isSkipped());
        QVERIFY(!checker.isOffered());
        QCOMPARE(found.size(), 0);

        // the next one is offered
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY(!checker.isSkipped());
        QVERIFY(checker.isOffered());
        QCOMPARE(found.size(), 1);
    }

    void skippingAfterwardsWorks()
    {
        FakeNetwork network;
        network.answers = {releases({u"0.7.0"_s})};
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY(checker.isOffered());
        checker.follow({.skipped = u"0.7.0"_s});
        QVERIFY(checker.isSkipped());
        QVERIFY(!checker.isOffered());
    }

    void failuresAreSaidInWords_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<QString>("said");
        QTest::newRow("down") << FakeAnswer{.error = QNetworkReply::HostNotFoundError}
                              << u"Couldn't reach GitHub"_s;
        QTest::newRow("slow") << FakeAnswer{.error = QNetworkReply::TimeoutError}
                              << u"didn't answer in time"_s;
        QTest::newRow("too often") << FakeAnswer{.status = 403} << u"asked too often"_s;
        QTest::newRow("missing") << FakeAnswer{.status = 404, .body = "{}"} << u"HTTP 404"_s;
        QTest::newRow("not a list") << FakeAnswer{.body = "{\"message\":\"hi\"}"} << u"couldn't be read"_s;
        QTest::newRow("not json") << FakeAnswer{.body = "<html>"} << u"couldn't be read"_s;
    }

    void failuresAreSaidInWords()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(QString, said);
        FakeNetwork network;
        network.answers = {answer};
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        QSignalSpy found(&checker, &UpdateChecker::found);
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY2(checker.errorString().contains(said), qPrintable(checker.errorString()));
        QVERIFY(!checker.newest());
        QVERIFY(!checker.checked().isValid());
        QCOMPARE(found.size(), 0);
    }

    void aGoodAnswerClearsTheFailureAndAFailureKeepsWhatItKnew()
    {
        FakeNetwork network;
        network.answers = {FakeAnswer{.error = QNetworkReply::HostNotFoundError}, releases({u"0.7.0"_s}),
                           FakeAnswer{.status = 500}};
        UpdateChecker checker(&network, v(u"0.6.0"_s));
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY(!checker.errorString().isEmpty());
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY(checker.errorString().isEmpty());
        QVERIFY(checker.isOffered());
        // GitHub down again doesn't take back what it said
        checker.check();
        QVERIFY(settles(checker));
        QVERIFY(!checker.errorString().isEmpty());
        QVERIFY(checker.isOffered());
    }

    void deletingItCallsOffTheRequest()
    {
        FakeNetwork network;
        network.answers = {FakeAnswer{.never = true}};
        {
            UpdateChecker checker(&network, v(u"0.6.0"_s));
            checker.check();
            QVERIFY(checker.isChecking());
        }
        QVERIFY(network.sent.at(0).reply.isNull());
    }
};

QTEST_GUILESS_MAIN(TestUpdateChecker)
#include "tst_updatechecker.moc"
