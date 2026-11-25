// SPDX-License-Identifier: MIT
#include "core/tradesite.h"

#include <QJsonDocument>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

QString text(const QUrl &url)
{
    return url.toString(QUrl::FullyEncoded);
}

} // namespace

class TestTradeSite : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dataEndpoints()
    {
        QCOMPARE(text(tradeDataUrl(u"stats"_s)), u"https://www.pathofexile.com/api/trade2/data/stats"_s);
        QCOMPARE(text(tradeDataUrl(u"leagues"_s)), u"https://www.pathofexile.com/api/trade2/data/leagues"_s);
    }

    void apiAddressesFollowTheBase()
    {
        const QUrl base(u"https://api.example.invalid/trade"_s);
        QCOMPARE(text(tradeApiUrl(base, tradeDataUrl(u"stats"_s))),
                 u"https://api.example.invalid/trade/data/stats"_s);
        QCOMPARE(text(tradeApiUrl(base, tradeSearchUrl(u"HC Rise of the Abyssal"_s))),
                 u"https://api.example.invalid/trade/search/poe2/HC%20Rise%20of%20the%20Abyssal"_s);
        QCOMPARE(text(tradeApiUrl(base, tradeFetchUrl({u"x1"_s, u"x2"_s}, u"Ab3dE9"_s))),
                 u"https://api.example.invalid/trade/fetch/x1,x2?query=Ab3dE9&realm=poe2"_s);
        // a slash on the end of the base doesn't double
        QCOMPARE(text(tradeApiUrl(QUrl(u"https://api.example.invalid/trade/"_s), tradeDataUrl(u"stats"_s))),
                 u"https://api.example.invalid/trade/data/stats"_s);
        QCOMPARE(text(tradeApiUrl(QUrl(u"https://api.example.invalid"_s), tradeDataUrl(u"stats"_s))),
                 u"https://api.example.invalid/data/stats"_s);
    }

    void apiAddressesAreNeverMadeForOtherPlaces()
    {
        const QUrl base(u"https://api.example.invalid/trade"_s);
        // not the site's API, or not the site
        QVERIFY(
            tradeApiUrl(base, QUrl(u"https://www.pathofexile.com/trade2/search/poe2/Standard"_s)).isEmpty());
        QVERIFY(tradeApiUrl(base, QUrl(u"https://elsewhere.invalid/api/trade2/data/stats"_s)).isEmpty());
        QVERIFY(tradeApiUrl(base, QUrl(u"http://www.pathofexile.com/api/trade2/data/stats"_s)).isEmpty());
        QVERIFY(tradeApiUrl(base, QUrl(u"https://www.pathofexile.com/api/trade2"_s)).isEmpty());
        // nor to a base that isn't https
        QVERIFY(tradeApiUrl(QUrl(u"http://api.example.invalid/trade"_s), tradeDataUrl(u"stats"_s)).isEmpty());
        QVERIFY(tradeApiUrl({}, tradeDataUrl(u"stats"_s)).isEmpty());
    }

    void search()
    {
        QCOMPARE(text(tradeSearchUrl(u"HC Rise of the Abyssal"_s)),
                 u"https://www.pathofexile.com/api/trade2/search/poe2/HC%20Rise%20of%20the%20Abyssal"_s);
        QCOMPARE(text(tradeSearchUrl(u"Odd/League"_s)),
                 u"https://www.pathofexile.com/api/trade2/search/poe2/Odd%2FLeague"_s);
    }

    void resultsPage()
    {
        QCOMPARE(text(tradeResultsUrl(u"Rise of the Abyssal"_s, u"abc"_s)),
                 u"https://www.pathofexile.com/trade2/search/poe2/Rise%20of%20the%20Abyssal/abc"_s);
    }

    void listings()
    {
        QCOMPARE(text(tradeFetchUrl({u"a1"_s, u"b2"_s}, u"Xy9"_s)),
                 u"https://www.pathofexile.com/api/trade2/fetch/a1,b2?query=Xy9&realm=poe2"_s);
    }

    void aFullPage()
    {
        QStringList ids;
        for (int n = 0; n < tradeFetchLimit; ++n)
            ids << u"id%1"_s.arg(n);
        const QString url = text(tradeFetchUrl(ids, u"Xy9"_s));
        QCOMPARE(url.count(u','), tradeFetchLimit - 1);
        QVERIFY(url.contains(u"id9?query=Xy9"));
    }

    void prefilledSearch()
    {
        const QJsonObject body{{u"query"_s, QJsonObject{{u"term"_s, u"+1 to Level & more"_s}}}};
        const QString url = text(tradePrefilledUrl(u"Standard"_s, body));
        const QString prefix = u"https://www.pathofexile.com/trade2/search/poe2/Standard?q="_s;
        QVERIFY(url.startsWith(prefix));

        // the page reads it back as a form field, where '+' is a space
        const QString q = url.mid(prefix.size());
        QVERIFY(!q.contains(u'+'));
        QVERIFY(!q.contains(u'&'));
        QCOMPARE(QJsonDocument::fromJson(QByteArray::fromPercentEncoding(q.toLatin1())).object(), body);
    }
};

QTEST_GUILESS_MAIN(TestTradeSite)
#include "tst_tradesite.moc"
