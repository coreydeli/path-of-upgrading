// SPDX-License-Identifier: MIT
#include "services/network.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestNetwork : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void userAgentSaysWhoItIs()
    {
        const QByteArray agent = userAgent();
        QVERIFY(agent.startsWith("PathOfUpgrading/"));
        QVERIFY(agent.endsWith(" (+https://github.com/coreydeli/path-of-upgrading)"));
    }

    void apiRequests()
    {
        const QNetworkRequest request = apiRequest(QUrl(u"https://poe.ninja/poe2/api/economy"_s));
        QCOMPARE(request.header(QNetworkRequest::UserAgentHeader).toByteArray(), userAgent());
        QCOMPARE(request.rawHeader("Accept"), "application/json");
        QVERIFY(request.transferTimeout() > 0);
    }

    void poeRequests()
    {
        const PoeConfig config{.clientId = u"examplepou"_s, .contact = u"example.invalid"_s, .ports = {}};
        const QByteArray agent = poeUserAgent(config);
        // "OAuth {clientId}/{version} (contact: {contact})" is what the API asks for
        QVERIFY(agent.startsWith("OAuth examplepou/"));
        QVERIFY(agent.contains(" (contact: example.invalid)"));
        QVERIFY(!agent.contains("pob"));

        const QNetworkRequest request = poeRequest(QUrl(u"https://api.pathofexile.com/x"_s), config);
        QCOMPARE(request.header(QNetworkRequest::UserAgentHeader).toByteArray(), agent);
        QCOMPARE(request.rawHeader("Accept"), "application/json");
        QVERIFY(request.transferTimeout() > 0);
    }
};

QTEST_GUILESS_MAIN(TestNetwork)
#include "tst_network.moc"
