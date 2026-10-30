// SPDX-License-Identifier: MIT
#include "core/league.h"

#include <QJsonArray>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

League league(const QString &name, bool hardcore = false, bool soloSelfFound = false)
{
    return {name, hardcore, soloSelfFound};
}

// Shaped like /api/trade2/data/leagues.
QJsonObject leaguesResponse()
{
    QJsonArray result;
    for (const QString &id : {u"Forbidden Rites"_s, u"HC Forbidden Rites"_s, u"Runes of Aldur"_s,
                              u"HC Runes of Aldur"_s, u"Standard"_s, u"Hardcore"_s})
        result.append(QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}, {u"text"_s, id}});
    return {{u"result"_s, result}};
}

} // namespace

class TestLeague : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void names_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<League>("expected");

        const QString runes = u"Runes of Aldur"_s;
        const QString standard = u"Standard"_s;
        QTest::newRow("league") << runes << league(runes);
        QTest::newRow("trade site hardcore") << u"HC Runes of Aldur"_s << league(runes, true);
        QTest::newRow("game hardcore") << u"Hardcore Runes of Aldur"_s << league(runes, true);
        QTest::newRow("ssf") << u"SSF Runes of Aldur"_s << league(runes, false, true);
        QTest::newRow("spelled out") << u"Solo Self-Found Runes of Aldur"_s << league(runes, false, true);
        QTest::newRow("hardcore ssf") << u"HC SSF Runes of Aldur"_s << league(runes, true, true);
        QTest::newRow("standard") << standard << league(standard);
        QTest::newRow("hardcore standard") << u"Hardcore"_s << league(standard, true);
        QTest::newRow("ssf standard") << u"SSF Standard"_s << league(standard, false, true);
        QTest::newRow("ssf hardcore") << u"SSF Hardcore"_s << league(standard, true, true);
        QTest::newRow("hardcore ssf standard") << u"Hardcore SSF"_s << league(standard, true, true);
        QTest::newRow("only starts like it") << u"Hcl Event"_s << league(u"Hcl Event"_s);
        QTest::newRow("loose spacing") << u"  hc  Runes of Aldur "_s << league(runes, true);
    }

    void names()
    {
        QFETCH(QString, text);
        QFETCH(League, expected);
        const League got = leagueFromName(text);
        QCOMPARE(got.name, expected.name);
        QCOMPARE(got.hardcore, expected.hardcore);
        QCOMPARE(got.soloSelfFound, expected.soloSelfFound);
    }

    void tradeIds()
    {
        QCOMPARE(league(u"Runes of Aldur"_s).tradeId(), u"Runes of Aldur"_s);
        QCOMPARE(league(u"Runes of Aldur"_s, true).tradeId(), u"HC Runes of Aldur"_s);
        QCOMPARE(league(u"Standard"_s).tradeId(), u"Standard"_s);
        QCOMPARE(league(u"Standard"_s, true).tradeId(), u"Hardcore"_s);
        QCOMPARE(league(u"Runes of Aldur"_s, true, true).tradeId(), QString());
        QVERIFY(league(u"Standard"_s, true).canTrade());
        QVERIFY(!league(u"Standard"_s, false, true).canTrade());
    }

    void displayNames()
    {
        QCOMPARE(league(u"Runes of Aldur"_s).displayName(), u"Runes of Aldur"_s);
        QCOMPARE(league(u"Runes of Aldur"_s, true, true).displayName(), u"Hardcore SSF Runes of Aldur"_s);
        QCOMPARE(league(u"Standard"_s, true).displayName(), u"Hardcore"_s);
        QCOMPARE(league(u"Standard"_s, false, true).displayName(), u"SSF Standard"_s);
        QCOMPARE(league(u"Standard"_s, true, true).displayName(), u"Hardcore SSF"_s);
    }

    void readsItsOwnNames()
    {
        for (const QString &name : {u"Runes of Aldur"_s, u"Standard"_s}) {
            for (const bool hardcore : {false, true}) {
                for (const bool soloSelfFound : {false, true}) {
                    const League l = league(name, hardcore, soloSelfFound);
                    QCOMPARE(leagueFromName(l.displayName()), l);
                    if (l.canTrade())
                        QCOMPARE(leagueFromName(l.tradeId()), l);
                }
            }
        }
    }

    void fromTheTradeSite()
    {
        QJsonObject response = leaguesResponse();
        QJsonArray result = response.value(u"result").toArray();
        result.append(QJsonObject{{u"id"_s, u"Settlers"_s}, {u"realm"_s, u"pc"_s}});
        response.insert(u"result"_s, result);

        const QStringList ids = tradeLeaguesFromJson(response);
        QCOMPARE(ids.size(), 6);
        QCOMPARE(ids.first(), u"Forbidden Rites"_s);
        QCOMPARE(leagueNames(ids), QStringList({u"Forbidden Rites"_s, u"Runes of Aldur"_s, u"Standard"_s}));
    }

    void standardGoesLast()
    {
        QCOMPARE(leagueNames({u"Standard"_s, u"Hardcore"_s, u"HC Runes of Aldur"_s, u"Runes of Aldur"_s}),
                 QStringList({u"Runes of Aldur"_s, u"Standard"_s}));
    }

    void followsTheCurrentLeague()
    {
        const QStringList ids = tradeLeaguesFromJson(leaguesResponse());
        QCOMPARE(resolveLeague(league({}, true), ids), league(u"Forbidden Rites"_s, true));
        QCOMPARE(resolveLeague(league(u"Runes of Aldur"_s, false, true), ids),
                 league(u"Runes of Aldur"_s, false, true));
        QCOMPARE(resolveLeague(league(u"Standard"_s, true), ids).tradeId(), u"Hardcore"_s);
        // one that has ended
        QCOMPARE(resolveLeague(league(u"Rise of the Abyssal"_s), ids), league(u"Forbidden Rites"_s));
    }

    void offline()
    {
        QCOMPARE(resolveLeague(league(u"Rise of the Abyssal"_s, true), {}),
                 league(u"Rise of the Abyssal"_s, true));
        QCOMPARE(resolveLeague(league({}), {}), league(u"Standard"_s));
    }
};

QTEST_GUILESS_MAIN(TestLeague)
#include "tst_league.moc"
