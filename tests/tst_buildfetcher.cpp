// SPDX-License-Identifier: MIT
#include "fakenetwork.h"
#include "services/buildfetcher.h"
#include "services/network.h"

#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestBuildFetcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // each failure is noted, which isn't wanted here
        QLoggingCategory::setFilterRules(u"pathofupgrading.pob.info=false"_s);
    }

    void fetchesTheCode()
    {
        FakeNetwork network;
        network.answers = {{.body = " eNrtCodeOfTheBuild\n"}};
        BuildFetcher fetcher(&network);
        QSignalSpy fetched(&fetcher, &BuildFetcher::fetched);

        QVERIFY(fetcher.fetch(u"https://pobb.in/AbC123"_s));
        QVERIFY(fetcher.isFetching());
        QVERIFY(fetched.wait());
        QCOMPARE(fetched.first().first().toString(), u"eNrtCodeOfTheBuild"_s);
        QVERIFY(!fetcher.isFetching());

        QCOMPARE(network.sent.size(), 1);
        const QNetworkRequest &request = network.sent.first().request;
        QCOMPARE(request.url(), QUrl(u"https://pobb.in/pob/AbC123"_s));
        QCOMPARE(request.header(QNetworkRequest::UserAgentHeader).toByteArray(), userAgent());
        QCOMPARE(request.transferTimeout(), 20000);
    }

    void notALink()
    {
        FakeNetwork network;
        BuildFetcher fetcher(&network);
        QVERIFY(!fetcher.fetch(u"eNrtCodeOfTheBuild"_s));
        QVERIFY(!fetcher.fetch(u"https://example.com/AbC123"_s));
        QVERIFY(!fetcher.isFetching());
        QVERIFY(network.sent.isEmpty());
    }

    void fails_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<QString>("why");
        QTest::newRow("no build") << FakeAnswer{.status = 404, .body = "Not found"}
                                  << u"There's no build at that link."_s;
        QTest::newRow("empty") << FakeAnswer{.body = " \n"} << u"There's no build at that link."_s;
        QTest::newRow("down") << FakeAnswer{.status = 503}
                              << u"pobb.in couldn't give the build (HTTP 503)."_s;
        QTest::newRow("unreachable") << FakeAnswer{.error = QNetworkReply::HostNotFoundError}
                                     << u"Couldn't reach pobb.in: Host pobb.in not found"_s;
        QTest::newRow("too slow") << FakeAnswer{.error = QNetworkReply::OperationCanceledError}
                                  << u"pobb.in didn't answer in time."_s;
        QTest::newRow("timed out") << FakeAnswer{.error = QNetworkReply::TimeoutError}
                                   << u"pobb.in didn't answer in time."_s;
    }

    void fails()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(QString, why);
        FakeNetwork network;
        network.answers = {answer};
        BuildFetcher fetcher(&network);
        QSignalSpy fetched(&fetcher, &BuildFetcher::fetched);
        QSignalSpy failed(&fetcher, &BuildFetcher::failed);

        QVERIFY(fetcher.fetch(u"https://pobb.in/AbC123"_s));
        QVERIFY(failed.wait());
        QCOMPARE(failed.first().first().toString(), why);
        QVERIFY(fetched.isEmpty());
        QVERIFY(!fetcher.isFetching());
    }

    void inPlaceOfTheOneBefore()
    {
        FakeNetwork network;
        network.answers = {{.body = "first"}, {.body = "second"}};
        BuildFetcher fetcher(&network);
        QSignalSpy fetched(&fetcher, &BuildFetcher::fetched);

        QVERIFY(fetcher.fetch(u"https://pobb.in/First"_s));
        QVERIFY(fetcher.fetch(u"https://poe.ninja/poe2/pob/Second"_s));
        QVERIFY(network.sent.first().reply.isNull());
        QVERIFY(fetched.wait());
        QCOMPARE(fetched.size(), 1);
        QCOMPARE(fetched.first().first().toString(), u"second"_s);
        QCOMPARE(network.sent.last().request.url(), QUrl(u"https://poe.ninja/poe2/pob/raw/Second"_s));
    }

    void goneWhileFetching()
    {
        FakeNetwork network;
        auto fetcher = std::make_unique<BuildFetcher>(&network);
        QVERIFY(fetcher->fetch(u"https://pobb.in/AbC123"_s));
        fetcher.reset();
        QVERIFY(network.sent.first().reply.isNull());
    }
};

QTEST_GUILESS_MAIN(TestBuildFetcher)
#include "tst_buildfetcher.moc"
