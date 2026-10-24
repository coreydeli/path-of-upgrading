// SPDX-License-Identifier: MIT
#include "app/settingsstore.h"
#include "app/tradeoptions.h"
#include "core/datafiles.h"
#include "fakenetwork.h"
#include "services/ratesstore.h"
#include "services/tradeclient.h"
#include "services/tradedatastore.h"
#include "tradefixtures.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <limits>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// The settings, with what the trade site and poe.ninja said earlier today.
struct Options
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    FakeNetwork network;
    TradeClient client{&network};
    TradeDataStore data{&client, dir.filePath(u"trade"_s)};
    RatesStore rates{&network, dir.filePath(u"rates"_s)};
    TradeOptions options{&settings, &data, &rates};

    // which the store reads, so neither site is asked
    bool hasLeagues(const QStringList &ids)
    {
        QJsonArray leagues;
        for (const QString &id : ids)
            leagues << QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}};
        for (const auto &[name, answer] : {std::pair{u"stats"_s, tradefixtures::stats()},
                                           std::pair{u"filters"_s, tradefixtures::filters()},
                                           std::pair{u"leagues"_s, QJsonObject{{u"result"_s, leagues}}}}) {
            if (!writeJsonFile(dir.filePath(u"trade/"_s + name + u".json"_s), answer))
                return false;
        }
        QSignalSpy refreshed(&data, &TradeDataStore::refreshed);
        data.refresh();
        return refreshed.wait() && network.sent.isEmpty();
    }

    bool hasRates(const QString &league, double exalts)
    {
        const QJsonObject core{{u"primary"_s, u"divine"_s},
                               {u"rates"_s, QJsonObject{{u"exalted"_s, exalts}}}};
        if (!writeJsonFile(dir.filePath(u"rates/"_s + league + u".json"_s),
                           {{u"core"_s, core}, {u"lines"_s, QJsonArray()}}))
            return false;
        QSignalSpy refreshed(&rates, &RatesStore::refreshed);
        rates.refresh(league);
        return refreshed.wait() && network.sent.isEmpty();
    }

    const TradeSettings &trade() const { return settings.settings().trade; }
};

const QStringList siteLeagues = {u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s, u"Hardcore"_s};

} // namespace

class TestTradeOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void readsTheSettings()
    {
        Options o;
        Settings settings;
        TradeSettings &trade = settings.trade;
        trade.enabled = false;
        trade.league = {.name = u"Runes of Aldur"_s, .hardcore = true, .soloSelfFound = true};
        trade.status = ListingStatus::Online;
        trade.budget = {.min = 2, .max = 30, .currency = Currency::Exalted, .divineRate = 250};
        trade.autoRate = false;
        QVERIFY(o.settings.set(settings));

        QCOMPARE(o.options.isEnabled(), false);
        QCOMPARE(o.options.league(), u"Runes of Aldur"_s);
        QCOMPARE(o.options.isHardcore(), true);
        QCOMPARE(o.options.isSoloSelfFound(), true);
        QCOMPARE(o.options.sellers(), u"online"_s);
        QCOMPARE(o.options.budgetMin(), 2.0);
        QCOMPARE(o.options.budgetMax(), 30.0);
        QCOMPARE(o.options.currency(), u"exalted"_s);
        QCOMPARE(o.options.divineRate(), 250.0);
        QCOMPARE(o.options.isAutoRate(), false);
    }

    void savesWhatYouChange()
    {
        Options o;
        QSignalSpy changed(&o.options, &TradeOptions::changed);
        o.options.setEnabled(false);
        o.options.setLeague(u" Runes of Aldur "_s);
        o.options.setHardcore(true);
        o.options.setSoloSelfFound(true);
        o.options.setSellers(u"securable"_s);
        o.options.setBudgetMin(1.5);
        o.options.setBudgetMax(20);
        o.options.setCurrency(u"chaos"_s);
        o.options.setDivineRate(310);
        o.options.setAutoRate(false);
        QCOMPARE(changed.size(), 10);

        TradeSettings trade;
        trade.enabled = false;
        trade.league = {.name = u"Runes of Aldur"_s, .hardcore = true, .soloSelfFound = true};
        trade.status = ListingStatus::Securable;
        trade.budget = {.min = 1.5, .max = 20, .currency = Currency::Chaos, .divineRate = 310};
        trade.autoRate = false;
        QCOMPARE(o.trade(), trade);
        // and it's in the file
        QCOMPARE(SettingsStore(o.dir.filePath(u"settings.conf"_s)).settings().trade, trade);

        // the same again changes nothing
        o.options.setHardcore(true);
        QCOMPARE(changed.size(), 10);
    }

    void andBack()
    {
        Options o;
        o.options.setEnabled(false);
        o.options.setHardcore(true);
        o.options.setSoloSelfFound(true);
        o.options.setAutoRate(false);
        o.options.setEnabled(true);
        o.options.setHardcore(false);
        o.options.setSoloSelfFound(false);
        o.options.setAutoRate(true);
        QCOMPARE(o.trade(), TradeSettings());
    }

    void unknownIds()
    {
        Options o;
        o.options.setSellers(u"online"_s);
        o.options.setCurrency(u"exalted"_s);
        o.options.setSellers(u"nobody"_s);
        o.options.setCurrency(u"gold"_s);
        QCOMPARE(o.trade().status, ListingStatus::Available);
        QCOMPARE(o.trade().budget.currency, Currency::Divine);
    }

    void amounts_data()
    {
        QTest::addColumn<double>("typed");
        QTest::addColumn<double>("kept");
        QTest::newRow("some") << 12.5 << 12.5;
        QTest::newRow("none") << 0.0 << 0.0;
        QTest::newRow("below none") << -3.0 << 0.0;
        QTest::newRow("not a number") << std::numeric_limits<double>::quiet_NaN() << 7.0;
        QTest::newRow("endless") << std::numeric_limits<double>::infinity() << 7.0;
    }

    void amounts()
    {
        QFETCH(double, typed);
        QFETCH(double, kept);
        Options o;
        o.options.setBudgetMin(7);
        o.options.setBudgetMax(7);
        o.options.setDivineRate(7);
        o.options.setBudgetMin(typed);
        o.options.setBudgetMax(typed);
        o.options.setDivineRate(typed);
        QCOMPARE(o.trade().budget.min, kept);
        QCOMPARE(o.trade().budget.max, kept);
        QCOMPARE(o.trade().budget.divineRate, kept);
    }

    void leagues_data()
    {
        QTest::addColumn<QStringList>("site");
        QTest::addColumn<QString>("yours");
        QTest::addColumn<QStringList>("leagues");
        const QStringList current = {u"Runes of Aldur"_s, u"Standard"_s};
        QTest::newRow("the site's") << siteLeagues << QString() << current;
        QTest::newRow("one of them") << siteLeagues << u"Runes of Aldur"_s << current;
        QTest::newRow("Standard") << siteLeagues << u"Standard"_s << current;
        QTest::newRow("ended") << siteLeagues << u"Dawn of the Hunt"_s
                               << QStringList{u"Runes of Aldur"_s, u"Dawn of the Hunt"_s, u"Standard"_s};
        QTest::newRow("before the site says") << QStringList() << QString() << QStringList{u"Standard"_s};
        QTest::newRow("yours before the site says") << QStringList() << u"Runes of Aldur"_s << current;
    }

    void leagues()
    {
        QFETCH(QStringList, site);
        QFETCH(QString, yours);
        QFETCH(QStringList, leagues);
        Options o;
        if (!site.isEmpty())
            QVERIFY(o.hasLeagues(site));
        o.options.setLeague(yours);
        QCOMPARE(o.options.leagues(), leagues);
    }

    void searchedLeague_data()
    {
        QTest::addColumn<QString>("yours");
        QTest::addColumn<bool>("hardcore");
        QTest::addColumn<QString>("searched");
        QTest::newRow("the current one") << QString() << false << u"Runes of Aldur"_s;
        QTest::newRow("hardcore") << QString() << true << u"Hardcore Runes of Aldur"_s;
        QTest::newRow("Standard") << u"Standard"_s << false << u"Standard"_s;
        QTest::newRow("hardcore Standard") << u"Standard"_s << true << u"Hardcore"_s;
        QTest::newRow("ended") << u"Dawn of the Hunt"_s << false << u"Runes of Aldur"_s;
    }

    void searchedLeague()
    {
        QFETCH(QString, yours);
        QFETCH(bool, hardcore);
        QFETCH(QString, searched);
        Options o;
        QVERIFY(o.hasLeagues(siteLeagues));
        o.options.setLeague(yours);
        o.options.setHardcore(hardcore);
        QCOMPARE(o.options.searchedLeague(), searched);
    }

    void unavailable()
    {
        Options o;
        QVERIFY(o.options.unavailable().isEmpty());
        o.options.setSoloSelfFound(true);
        QCOMPARE(o.options.unavailable(),
                 u"There's no trading in solo self-found, so trade searches are off."_s);
        o.options.setEnabled(false);
        QCOMPARE(o.options.unavailable(), u"Trade searches are turned off in the settings."_s);
    }

    void ninjaRate()
    {
        Options o;
        QVERIFY(o.hasLeagues(siteLeagues));
        QCOMPARE(o.options.ninjaRate(), 0.0);
        QSignalSpy changed(&o.options, &TradeOptions::changed);
        QVERIFY(o.hasRates(u"Runes of Aldur"_s, 300));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(o.options.ninjaRate(), 300.0);
        // until they're the hardcore league's
        o.options.setHardcore(true);
        QCOMPARE(o.options.ninjaRate(), 0.0);
        QVERIFY(o.hasRates(u"HC Runes of Aldur"_s, 450));
        QCOMPARE(o.options.ninjaRate(), 450.0);
    }

    void siteLeaguesComeIn()
    {
        Options o;
        QSignalSpy changed(&o.options, &TradeOptions::changed);
        QVERIFY(o.hasLeagues(siteLeagues));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(o.options.searchedLeague(), u"Runes of Aldur"_s);
    }
};

QTEST_GUILESS_MAIN(TestTradeOptions)
#include "tst_tradeoptions.moc"
