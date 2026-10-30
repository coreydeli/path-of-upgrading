// SPDX-License-Identifier: MIT
#include "app/itemchecker.h"
#include "app/tradereport.h"
#include "core/datafiles.h"
#include "core/parser.h"
#include "core/placement.h"
#include "fakegameinput.h"
#include "fakenetwork.h"
#include "fakepob.h"
#include "itemtexts.h"
#include "services/pobengine.h"
#include "services/ratesstore.h"
#include "services/tradeclient.h"
#include "services/tradedatastore.h"
#include "services/tradesearcher.h"
#include "tradefixtures.h"

#include <QGuiApplication>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

namespace {

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/></PathOfBuilding2>"_s;

ScreenPoint mouseAt(int x, int y)
{
    return {.screen = QGuiApplication::primaryScreen()->name(), .position = {x, y}};
}

// A point on the popup, in the screen's coordinates, moved by the offset.
ScreenPoint onPopup(const ItemPopup &popup, QPoint offset = {})
{
    QScreen *screen = QGuiApplication::primaryScreen();
    return {.screen = screen->name(),
            .position = popup.window()->geometry().center() - screen->geometry().topLeft() + offset};
}

// Where the popup should be for the mouse, on the screen.
QPoint placed(const ItemPopup &popup, PopupPosition where, const std::optional<QPoint> &mouse)
{
    QScreen *screen = QGuiApplication::primaryScreen();
    return screen->geometry().topLeft() + placePopup(where, popup.window()->size(), screen->size(), mouse);
}

Character inGrimCrown()
{
    Character c;
    c.gear = {{u"Helmet"_s, parseItem(itemtexts::helmetOld).value()}};
    return c;
}

// Path of Building's answer for the helmet, with the build's numbers as they
// are and with it on.
QJsonObject withHelmet(const QJsonObject &stats)
{
    const QJsonObject before{{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1000}};
    const QJsonObject place{{u"slot"_s, u"Helmet"_s},
                            {u"label"_s, u"Helmet"_s},
                            {u"replacing"_s, u"Grim Crown"_s},
                            {u"stats"_s, stats}};
    return {{u"before"_s, before},
            {u"results"_s, QJsonArray{QJsonObject{{u"ok"_s, true}, {u"slots"_s, QJsonArray{place}}}}}};
}

// The checker with Path of Building, which has the build.
struct WithPob
{
    QQmlEngine qml;
    FakeGameInput input;
    FakePob pob;
    PobEngine engine;
    ItemChecker checker{&qml, &input};

    WithPob()
    {
        engine.setInstall(pob.install());
        engine.setBuild(build);
        checker.setPob(&engine);
        checker.setCharacter(inGrimCrown(), PobBuild());
    }

    // the report on the helmet once Path of Building has answered
    ItemReport answered(const QJsonObject &stats)
    {
        pob.answers(withHelmet(stats));
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        if (!QTest::qWaitFor([&] { return !checker.popup().report().pobChecking; }))
            return {};
        return checker.popup().report();
    }
};

// The trade site, which the popup's buttons search.
struct Trading
{
    QTemporaryDir dir;
    FakeNetwork network;
    TradeClient client{&network};
    TradeDataStore data{&client, dir.path()};
    TradeSearcher searcher{&client};
    FakeNetwork ninja; // poe.ninja
    RatesStore rates{&ninja, dir.filePath(u"rates"_s)};
    TradeFinder finder{&searcher, &data, &rates};

    explicit Trading(ItemChecker &checker)
    {
        checker.setFinder(&finder);
        checker.popup().setTradeAvailable(true);
    }

    // what the site said of its stats earlier today, which the store reads
    bool hasData()
    {
        const QJsonObject leagues = QJsonDocument::fromJson(R"({"result":[{"id":"Standard"}]})").object();
        for (const auto &[name, answer] :
             {std::pair{u"stats"_s, tradefixtures::stats()},
              std::pair{u"filters"_s, tradefixtures::filters()}, std::pair{u"leagues"_s, leagues}}) {
            if (!writeJsonFile(dir.filePath(name + u".json"_s), answer))
                return false;
        }
        QSignalSpy refreshed(&data, &TradeDataStore::refreshed);
        data.refresh();
        return refreshed.wait() && network.sent.isEmpty();
    }
};

} // namespace

class TestItemChecker : public QObject
{
    Q_OBJECT

public:
    // the popup's windows aren't for the screen
    static void initMain() { qputenv("QT_QPA_PLATFORM", "offscreen"); }

private Q_SLOTS:
    void initTestCase()
    {
        // what they say when poe.ninja has no rates, which these tests don't need,
        // and when the trade site lets a search down, which one does
        QLoggingCategory::setFilterRules(u"pathofupgrading.ninja.warning=false\n"
                                         u"pathofupgrading.trade.warning=false"_s);
    }

    void init() { QTest::failOnWarning(); }

    void showsIt()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.setCharacter(inGrimCrown(), std::nullopt);
        QSignalSpy read(&checker, &ItemChecker::itemRead);
        checker.check();
        QCOMPARE(input.copies, 1);
        QVERIFY(!checker.popup().isVisible());

        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().report().name, u"Doom Visage"_s);
        QCOMPARE(checker.popup().report().versus, u"vs Grim Crown"_s);
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Cursor, QPoint(200, 300)));
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().at(0).value<Item>().name, u"Doom Visage"_s);
    }

    void hidesThePopupInTheWay()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.check();
        QVERIFY(!input.lastNudge);
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(checker.popup().isVisible());

        // the mouse is on it, so the game would see that and not the item
        input.mouseAt = onPopup(checker.popup());
        Q_EMIT input.copyFailed(GameInput::Failure::NothingCopied, {}, std::nullopt);
        checker.check();
        QVERIFY(!checker.popup().isVisible());
        QCOMPARE(input.copies, 2);
        QVERIFY(input.lastNudge);

        // and the next item brings it back
        Q_EMIT input.itemCopied(itemtexts::helmetOld, mouseAt(200, 300));
        QVERIFY(checker.popup().isVisible());
    }

    void hidesThePopupHovered()
    {
        // which is how it's known over the popup, where the mouse isn't
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        checker.popup().setHovered(true);
        QVERIFY(!input.mouseAt);
        Q_EMIT input.copyFailed(GameInput::Failure::NothingCopied, {}, std::nullopt);
        checker.check();
        QVERIFY(!checker.popup().isVisible());
        QVERIFY(input.lastNudge);
    }

    void leavesThePopupNotInTheWay()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));

        // with the mouse well off it
        input.mouseAt = onPopup(checker.popup(), {checker.popup().window()->width(), 0});
        Q_EMIT input.copyFailed(GameInput::Failure::NothingCopied, {}, std::nullopt);
        checker.check();
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(input.copies, 2);
        QVERIFY(!input.lastNudge);
    }

    void leavesThePopupWhenItCantRead()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        checker.popup().setHovered(true);

        Settings paused;
        paused.overlay.paused = true;
        checker.setSettings(paused);
        checker.check();
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(input.copies, 1);

        // and while it reads one already
        checker.setSettings({});
        checker.check();
        QVERIFY(!checker.popup().isVisible());
        QCOMPARE(input.copies, 2);
        QTRY_VERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().message(), u"The game is still copying the item."_s);
        checker.popup().setHovered(true);
        checker.check();
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(input.copies, 2);
    }

    void wear()
    {
        // as the popup's buttons do
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.setCharacter(inGrimCrown(), std::nullopt);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QCOMPARE(checker.popup().report().wearIn, QStringList{u"Helmet"_s});
        QSignalSpy asked(&checker, &ItemChecker::wearAsked);
        checker.popup().wear(u"Helmet"_s);
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.first().at(0).value<Item>().name, u"Doom Visage"_s);
        QCOMPARE(asked.first().at(1).toString(), u"Helmet"_s);
    }

    void resultsAsked()
    {
        // as the popup's button does, from the screen the item was read on
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew,
                                ScreenPoint{.screen = u"DP-1"_s, .position = {200, 300}});
        QSignalSpy asked(&checker, &ItemChecker::resultsAsked);
        checker.popup().showResults();
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.first().first().toString(), u"DP-1"_s);
    }

    void judgedAgain()
    {
        // for the character you play now, as once you wear it
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.setCharacter(inGrimCrown(), std::nullopt);
        QSignalSpy read(&checker, &ItemChecker::itemRead);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QCOMPARE(checker.popup().report().versus, u"vs Grim Crown"_s);
        checker.popup().setExpanded(true);

        Character worn;
        worn.gear = {{u"Helmet"_s, parseItem(itemtexts::helmetNew).value()}};
        checker.setCharacter(worn, std::nullopt);
        QCOMPARE(checker.popup().report().verdict, u"Equipped"_s);
        QCOMPARE(checker.popup().report().versus, u"in your Helmet slot"_s);
        QVERIFY(checker.popup().report().wearIn.isEmpty());
        // which isn't another item read
        QCOMPARE(read.size(), 1);
        QVERIFY(checker.popup().isExpanded());
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Cursor, QPoint(200, 300)));
    }

    void notJudgedAgain()
    {
        // once you've closed it
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.setCharacter(inGrimCrown(), std::nullopt);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        checker.popup().close();
        QSignalSpy shown(&checker.popup(), &ItemPopup::shownChanged);
        checker.setCharacter({}, std::nullopt);
        QCOMPARE(shown.size(), 0);
        QVERIFY(!checker.popup().isVisible());
        QCOMPARE(checker.popup().report().versus, u"vs Grim Crown"_s);
    }

    void findsOnTrade()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Trading trading(checker);
        QVERIFY(trading.hasData());
        trading.network.answers << FakeAnswer{.body = R"({"id":"Ab3dE9","total":3,"result":["a","b","c"]})"};
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));

        // as the button does
        QSignalSpy finished(&trading.finder, &TradeFinder::finished);
        checker.popup().findOnTrade(TradeFinder::Kind::Similar);
        QCOMPARE(checker.popup().trade(), searchingReport());
        QVERIFY(finished.wait());
        const TradeFinder::Found &found = *trading.finder.found();
        QCOMPARE(found.item.name, u"Doom Visage"_s);
        QCOMPARE(found.kind, TradeFinder::Kind::Similar);
        QCOMPARE(checker.popup().trade(), reportOf(found));
        QCOMPARE(checker.popup().trade().status, u"3 listed in Standard"_s);
    }

    void notOnTrade()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Trading trading(checker);
        QVERIFY(trading.hasData());
        TradeSettings off;
        off.enabled = false;
        trading.searcher.setSettings(off);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QSignalSpy finished(&trading.finder, &TradeFinder::finished);
        checker.popup().findOnTrade(TradeFinder::Kind::Upgrades);
        QVERIFY(finished.wait());
        QCOMPARE(checker.popup().trade(), failedReport(u"Trade searches are turned off in the settings."_s));
    }

    // the site couldn't be searched, so the popup offers it for you to try
    void tradeSiteFails()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Trading trading(checker);
        QVERIFY(trading.hasData());
        trading.network.answers << FakeAnswer{.error = QNetworkReply::HostNotFoundError};
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QSignalSpy finished(&trading.finder, &TradeFinder::finished);
        checker.popup().findOnTrade(TradeFinder::Kind::Similar);
        QVERIFY(finished.wait());

        const TradeReport trade = checker.popup().trade();
        QVERIFY(trade.failed);
        QVERIFY(!trade.listed);
        QCOMPARE(trade.status, trading.finder.errorString());
        QVERIFY(!trade.url.isEmpty());
        QCOMPARE(trade, failedReport(trading.finder.errorString(), trading.finder.errorUrl()));
        QVERIFY(trade.url.path().startsWith(u"/trade2/search/poe2/Standard"_s));
        QVERIFY(trade.url.hasQuery());
    }

    void tradeWaitsItsTurn()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Trading trading(checker);
        QVERIFY(trading.hasData());
        // the site takes four searches in ten seconds until it says otherwise
        for (int i = 0; i < 4; ++i) {
            trading.network.answers << FakeAnswer{.body = R"({"id":"Ab3dE9","total":0,"result":[]})"};
            trading.searcher.search({});
        }
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        checker.popup().findOnTrade(TradeFinder::Kind::Similar);
        QSignalSpy waiting(&trading.finder, &TradeFinder::waiting);
        QVERIFY(waiting.wait());
        QCOMPARE(checker.popup().trade(),
                 searchingReport(waiting.first().first().value<std::chrono::milliseconds>()));
        QVERIFY(checker.popup().trade().status.startsWith(u"Waiting"));
        trading.finder.cancel();
    }

    void nextItemCallsItOff()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Trading trading(checker);
        QVERIFY(trading.hasData());
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QSignalSpy finished(&trading.finder, &TradeFinder::finished);
        checker.popup().findOnTrade(TradeFinder::Kind::Similar);
        QVERIFY(trading.finder.isBusy());
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetOld, mouseAt(200, 300));
        QVERIFY(!trading.finder.isBusy());
        QVERIFY(!finished.wait(50));
        QCOMPARE(checker.popup().trade(), TradeReport());
    }

    void finderGone()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        {
            Trading trading(checker);
        }
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        checker.popup().findOnTrade(TradeFinder::Kind::Similar);
        QCOMPARE(checker.popup().trade(), TradeReport());
    }

    void noFinder()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        checker.popup().findOnTrade(TradeFinder::Kind::Similar);
        QCOMPARE(checker.popup().trade(), TradeReport());
        // and the next item is read as ever
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetOld, mouseAt(200, 300));
        QCOMPARE(checker.popup().report().name, u"Grim Crown"_s);
    }

    void slowGame()
    {
        QQmlEngine qml;
        FakeGameInput input;
        input.mouseAt = mouseAt(200, 300);
        ItemChecker checker(&qml, &input);
        checker.check();
        QTRY_VERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().message(), u"The game is still copying the item."_s);
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Cursor, QPoint(200, 300)));

        // and once it has
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QCOMPARE(checker.popup().message(), QString());
        QCOMPARE(checker.popup().report().name, u"Doom Visage"_s);
    }

    void failed()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        input.failRightAway = u"The game isn't running."_s;
        checker.check();
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().message(), u"The game isn't running."_s);
    }

    void yourHotkey()
    {
        // which it tells you to press again
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.hotkey = u"F3"_s;
        checker.setSettings(settings);
        checker.check();
        Q_EMIT input.copyFailed(GameInput::Failure::NothingCopied, {}, mouseAt(200, 300));
        QVERIFY(checker.popup().message().contains(u"press F3"_s));
    }

    void advancedCopy()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.advancedCopy = true;
        checker.setSettings(settings);
        checker.check();
        QVERIFY(input.lastAdvanced);
    }

    void paused()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.paused = true;
        checker.setSettings(settings);
        checker.check();
        QCOMPARE(input.copies, 0);
    }

    void copiedYourself()
    {
        // where the mouse isn't known
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Q_EMIT input.textCopied(itemtexts::helmetNew);
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().report().name, u"Doom Visage"_s);
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Top, std::nullopt));
    }

    void checkedAgain()
    {
        // an item you have already, with no mouse to go by
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.setCharacter(inGrimCrown(), std::nullopt);
        QSignalSpy read(&checker, &ItemChecker::itemRead);
        checker.checkItem(parseItem(itemtexts::helmetNew).value());
        QCOMPARE(input.copies, 0);
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().report().name, u"Doom Visage"_s);
        QCOMPARE(checker.popup().report().versus, u"vs Grim Crown"_s);
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Top, std::nullopt));
        // as an item read, for the history
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().at(0).value<Item>().name, u"Doom Visage"_s);

        // for the character you play now, and wearable as it is
        Character worn;
        worn.gear = {{u"Helmet"_s, parseItem(itemtexts::helmetNew).value()}};
        checker.setCharacter(worn, std::nullopt);
        checker.checkItem(parseItem(itemtexts::helmetOld).value());
        QCOMPARE(checker.popup().report().name, u"Grim Crown"_s);
        QCOMPARE(checker.popup().report().versus, u"vs Doom Visage"_s);
        QSignalSpy asked(&checker, &ItemChecker::wearAsked);
        checker.popup().wear(u"Helmet"_s);
        QCOMPARE(asked.first().at(0).value<Item>().name, u"Grim Crown"_s);
    }

    void checkedAgainAway()
    {
        // from where the mouse was, or is, to where your settings put it
        QQmlEngine qml;
        FakeGameInput input;
        input.mouseAt = mouseAt(500, 400);
        ItemChecker checker(&qml, &input);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        const QPoint byMouse = checker.popup().window()->position();
        QCOMPARE(byMouse, placed(checker.popup(), PopupPosition::Cursor, QPoint(200, 300)));

        checker.checkItem(parseItem(itemtexts::helmetOld).value());
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Top, std::nullopt));
        QCOMPARE_NE(checker.popup().window()->position(), byMouse);

        Settings settings;
        settings.overlay.position = PopupPosition::Right;
        checker.setSettings(settings);
        checker.checkItem(parseItem(itemtexts::helmetNew).value());
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Right, std::nullopt));
    }

    void checkedAgainWithPob()
    {
        WithPob p;
        p.pob.answers(withHelmet({{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}}));
        p.checker.checkItem(parseItem(itemtexts::helmetNew).value());
        QVERIFY(p.checker.popup().report().pobChecking);
        QTRY_VERIFY(!p.checker.popup().report().pobChecking);
        // and its answer leaves the popup where it was
        QCOMPARE(p.checker.popup().window()->position(),
                 placed(p.checker.popup(), PopupPosition::Top, std::nullopt));
    }

    void checkedAgainCallsOffTheSearch()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Trading trading(checker);
        QVERIFY(trading.hasData());
        checker.checkItem(parseItem(itemtexts::helmetNew).value());
        checker.popup().findOnTrade(TradeFinder::Kind::Similar);
        QVERIFY(trading.finder.isBusy());
        checker.checkItem(parseItem(itemtexts::helmetOld).value());
        QVERIFY(!trading.finder.isBusy());
        QCOMPARE(checker.popup().trade(), TradeReport());
    }

    void notWatching()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.watchClipboard = false;
        checker.setSettings(settings);
        Q_EMIT input.textCopied(itemtexts::helmetNew);
        QVERIFY(!checker.popup().isVisible());
    }

    void popupSettings()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.scale = 1.5;
        checker.setSettings(settings);
        QCOMPARE(checker.popup().scale(), 1.5);
    }

    void pobAnswers()
    {
        WithPob p;
        p.pob.answers(withHelmet({{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}}));
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(p.checker.popup().report().pobChecking);

        QTRY_VERIFY(!p.checker.popup().report().pobChecking);
        QCOMPARE(p.checker.popup().report().name, u"Doom Visage"_s);
        QVERIFY(!p.checker.popup().report().headline.isEmpty());
        // still by the mouse where you checked it
        QTRY_COMPARE(p.checker.popup().window()->position(),
                     placed(p.checker.popup(), PopupPosition::Cursor, QPoint(200, 300)));
    }

    void staysClosed()
    {
        // when you close it before Path of Building answers
        WithPob p;
        p.pob.answers(withHelmet({{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}}));
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        p.checker.popup().close();
        QTRY_VERIFY(!p.checker.popup().report().pobChecking);
        QVERIFY(!p.checker.popup().isVisible());
    }

    void staysOpen()
    {
        // with the rest showing, when you opened it up first
        WithPob p;
        p.pob.answers(withHelmet({{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}}));
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        p.checker.popup().setExpanded(true);
        QTRY_VERIFY(!p.checker.popup().report().pobChecking);
        QVERIFY(p.checker.popup().isExpanded());
    }

    void pobOff()
    {
        WithPob p;
        Settings settings;
        settings.pob.enabled = false;
        p.checker.setSettings(settings);
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(!p.checker.popup().report().pobChecking);

        // and once it's on again
        p.checker.setSettings({});
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(p.checker.popup().report().pobChecking);
    }

    void pobOffFirst()
    {
        // before there's a Path of Building to use
        QQmlEngine qml;
        FakeGameInput input;
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        ItemChecker checker(&qml, &input);
        checker.setCharacter(inGrimCrown(), PobBuild());
        Settings settings;
        settings.pob.enabled = false;
        checker.setSettings(settings);
        checker.setPob(&engine);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(!checker.popup().report().pobChecking);
    }

    void yourDamageWeight()
    {
        // 10% more damage and no more life
        const QJsonObject harder{{u"FullDPS"_s, 110}, {u"TotalEHP"_s, 1000}};
        WithPob p;
        QCOMPARE(p.answered(harder).verdict, u"Upgrade"_s);

        Settings settings;
        settings.pob.damageWeight = 0;
        p.checker.setSettings(settings);
        QCOMPARE_NE(p.answered(harder).verdict, u"Upgrade"_s);
    }

    void yourPriorities()
    {
        // a fifth of your damage for half again as much life
        const QJsonObject tougher{{u"FullDPS"_s, 80}, {u"TotalEHP"_s, 1500}};
        WithPob p;
        QVERIFY(!p.answered(tougher).losses.isEmpty());

        Settings settings;
        settings.pob.priorities.keepDamage = false;
        p.checker.setSettings(settings);
        QVERIFY(p.answered(tougher).losses.isEmpty());
    }
};

QTEST_MAIN(TestItemChecker)
#include "tst_itemchecker.moc"
