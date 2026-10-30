// SPDX-License-Identifier: MIT
#include "app/itemreport.h"
#include "app/resultspanel.h"
#include "core/placement.h"
#include "core/resists.h"
#include "fakegameinput.h"
#include "fakepob.h"
#include "resultsite.h"
#include "services/pobengine.h"

#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QtMath>
#include <QtQuickTest/quicktest.h>
#include <algorithm>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using namespace resultsite;

namespace {

void addVisible(QQuickItem *parent, QList<QQuickItem *> &items)
{
    const QList<QQuickItem *> children = parent->childItems();
    for (QQuickItem *item : children) {
        if (item->isVisible()) {
            items << item;
            addVisible(item, items);
        }
    }
}

// What you can see in the window.
QList<QQuickItem *> visibleItems(const QQuickWindow *window)
{
    QList<QQuickItem *> items;
    addVisible(window->contentItem(), items);
    return items;
}

// What in the window you can see with that text, a label or a button.
QQuickItem *shown(const QQuickWindow *window, const QString &text, const char *type = "QQuickItem")
{
    const QList<QQuickItem *> items = visibleItems(window);
    const auto it = std::ranges::find_if(items, [&](const QQuickItem *item) {
        return item->inherits(type) && item->property("text").toString() == text;
    });
    return it != items.end() ? *it : nullptr;
}

QQuickItem *button(const QQuickWindow *window, const QString &text)
{
    return shown(window, text, "QQuickAbstractButton");
}

// How many listings are in the window's list.
int rows(const QQuickWindow *window)
{
    const QList<QQuickItem *> items = visibleItems(window);
    const auto it =
        std::ranges::find_if(items, [](const QQuickItem *item) { return item->inherits("QQuickListView"); });
    return it != items.end() ? (*it)->property("count").toInt() : 0;
}

// Clicks the button with that text, once it's there and laid out.
bool click(QQuickWindow *window, const QString &text)
{
    const auto ready = [&] {
        const QQuickItem *b = button(window, text);
        return b && b->isEnabled();
    };
    if (!QTest::qWaitFor(ready, 3000) || !QQuickTest::qWaitForPolish(window))
        return false;
    const QQuickItem *b = button(window, text);
    const QPoint middle = b->mapToScene(b->boundingRect().center()).toPoint();
    QTest::mouseClick(window, Qt::LeftButton, {}, middle);
    return true;
}

// Takes the links the window opens, in place of the browser.
class Browser : public QObject
{
    Q_OBJECT

public:
    Browser() { QDesktopServices::setUrlHandler(u"https"_s, this, "open"); }
    ~Browser() override { QDesktopServices::unsetUrlHandler(u"https"_s); }

    QList<QUrl> opened;

public Q_SLOTS:
    void open(const QUrl &url) { opened << url; }
};

// The results, the window for them over a game that doesn't say where the
// mouse is, and the trade site that has the listings.
struct Panel
{
    Site site;
    QQmlEngine qml;
    FakeGameInput input;
    ResultsPanel panel{&qml, &input, &site.results};

    // after the window has asked for the listings and got them
    bool loaded() { return site.waitsForIt(); }

    QQuickWindow *window() const { return panel.window(); }
};

QScreen *screen()
{
    return QGuiApplication::primaryScreen();
}

// In a helmet with less life than the listed ones, and these mods.
Character wearing(const QString &mods)
{
    const QString helmet =
        u"Item Class: Helmets\nRarity: Rare\nGrim Crown\nIron Hat\n--------\nItem Level: 80\n--------\n"_s;
    Character c;
    c.gear = {{u"Helmet"_s, parseItem(helmet + mods).value()}};
    return c;
}

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/></PathOfBuilding2>"_s;

int toneOf(const QQuickItem *label)
{
    return label->property("tone").toInt();
}

} // namespace

class TestResultsPanel : public QObject
{
    Q_OBJECT

public:
    // the window isn't for the screen
    static void initMain() { qputenv("QT_QPA_PLATFORM", "offscreen"); }

private Q_SLOTS:
    void initTestCase()
    {
        // what they say when the site or Path of Building lets them down,
        // which the tests make them do
        QLoggingCategory::setFilterRules(u"pathofupgrading.trade.warning=false\n"
                                         u"pathofupgrading.trade.info=false\n"
                                         u"pathofupgrading.ninja.debug=false\n"
                                         u"pathofupgrading.pob.warning=false\n"
                                         u"pathofupgrading.pob.info=false"_s);
    }

    void init()
    {
        // which a mistake in the QML only gives
        QTest::failOnWarning();
    }

    void startsHidden()
    {
        Panel p;
        QVERIFY(!p.panel.isVisible());
        QVERIFY(!p.panel.window());
        p.panel.close();
        QVERIFY(p.site.network.sent.isEmpty());
    }

    void nothingToShowYet()
    {
        Panel p;
        p.panel.show();
        QVERIFY(p.panel.isVisible());
        // over the game, which keeps the keyboard
        QVERIFY(p.window()->flags().testFlag(Qt::WindowDoesNotAcceptFocus));
        const QString first = u"Check an item and search the trade site from the popup first."_s;
        QVERIFY(shown(p.window(), first));
        QVERIFY(shown(p.window(), u"Trade Results"_s));
        QVERIFY(!button(p.window(), u"Open on the Trade Site"_s));
        QVERIFY(!button(p.window(), u"Load More"_s));
        QCOMPARE(rows(p.window()), 0);

        // or why there won't be
        const QString off = u"Trade searches are turned off in the settings."_s;
        p.panel.setUnavailable(off);
        QVERIFY(shown(p.window(), off));
        QVERIFY(!shown(p.window(), first));
        QVERIFY(p.site.network.sent.isEmpty());
    }

    void saysWhatChanged()
    {
        Panel p;
        QSignalSpy unavailable(&p.panel, &ResultsPanel::unavailableChanged);
        QSignalSpy height(&p.panel, &ResultsPanel::maxHeightChanged);
        const QString off = u"Trade searches are turned off in the settings."_s;
        p.panel.setUnavailable(off);
        p.panel.setUnavailable(off);
        QCOMPARE(unavailable.size(), 1);
        p.panel.setUnavailable({});
        QCOMPARE(unavailable.size(), 2);

        // the height is for the screen, so it's only new the first time
        QCOMPARE(height.size(), 0);
        p.panel.show();
        QCOMPARE(height.size(), 1);
        p.panel.close();
        p.panel.show();
        QCOMPARE(height.size(), 1);
    }

    void showsTheListings()
    {
        Panel p;
        p.site.network.answers = {fetched(listingIds(2))};
        QVERIFY(p.site.hasRates(700));
        const TradeFinder::Found found = foundBy(2);
        p.site.results.setSearch(found);
        // asked for when you want to see them
        QVERIFY(p.site.network.sent.isEmpty());
        p.panel.show();
        QVERIFY(p.panel.isVisible());
        QVERIFY(p.loaded());
        QCOMPARE(p.site.network.sent.size(), 1);

        QTRY_COMPARE(rows(p.window()), 2);
        QVERIFY(QQuickTest::qWaitForPolish(p.window()));
        QVERIFY(shown(p.window(), u"Same stats or better than Doom Visage"_s));
        QVERIFY(shown(p.window(), u"2 listed in Runes of Aldur"_s));
        QVERIFY(shown(p.window(), u"Grim Ward"_s));
        QVERIFY(shown(p.window(), u"Iron Hat"_s));
        QVERIFY(shown(p.window(), u"3 div"_s));
        QVERIFY(shown(p.window(), u"about 2100 ex"_s));
        QVERIFY(shown(p.window(), u"+80 to maximum Life"_s));
        QVERIFY(shown(p.window(), u"Cannot be Frozen"_s));
        QVERIFY(shown(p.window(), u"Sellerx1#1234"_s));
        QVERIFY(shown(p.window(), u"Online"_s));
        QVERIFY(button(p.window(), u"Open on the Trade Site"_s));
        // with no more to load
        QVERIFY(!button(p.window(), u"Load More"_s));
        QVERIFY(!shown(p.window(), u"Asking the trade site for the listings"_s));
    }

    void bigTotal()
    {
        Panel p;
        p.site.network.answers = {fetched(listingIds(10))};
        p.site.results.setSearch(foundBy(1234));
        p.panel.show();
        QVERIFY(p.loaded());
        QVERIFY(shown(p.window(), u"1,234 listed in Runes of Aldur"_s));
    }

    void onlineAndWhenListed()
    {
        Panel p;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const QJsonObject cost = price(3, u"divine"_s);
        // one that's here, and one that's gone
        const QJsonObject here = helmet(u"x1"_s, cost, now.addSecs(-5 * 60).toString(Qt::ISODate));
        QJsonObject away = helmet(u"x2"_s, cost, now.addSecs(-3 * 3600).toString(Qt::ISODate));
        QJsonObject listing = away[u"listing"_s].toObject();
        QJsonObject account = listing[u"account"_s].toObject();
        account.remove(u"online"_s);
        listing[u"account"_s] = account;
        away[u"listing"_s] = listing;
        p.site.network.answers = {{.body = json({{u"result"_s, QJsonArray{here, away}}})}};
        p.site.results.setSearch(foundBy(2));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 2);

        QVERIFY(shown(p.window(), u"5 minutes ago"_s));
        QVERIFY(shown(p.window(), u"3 hours ago"_s));
        const QQuickItem *online = shown(p.window(), u"Online"_s);
        const QQuickItem *offline = shown(p.window(), u"Offline"_s);
        QVERIFY(online);
        QVERIFY(offline);
        QCOMPARE(online->property("tone").toInt(), int(Tone::Good));
        QCOMPARE(offline->property("tone").toInt(), int(Tone::Plain));
    }

    void ratesComeLater()
    {
        // and the prices are worked out when they do
        Panel p;
        p.site.network.answers = {fetched(listingIds(1))};
        p.site.results.setSearch(foundBy(1));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 1);
        QVERIFY(QQuickTest::qWaitForPolish(p.window()));
        QVERIFY(shown(p.window(), u"3 div"_s));
        QVERIFY(!shown(p.window(), u"about 2100 ex"_s));
        QVERIFY(p.site.hasRates(700));
        QTRY_VERIFY(shown(p.window(), u"about 2100 ex"_s));
    }

    void loadsMore()
    {
        Panel p;
        const QStringList ids = listingIds(23);
        p.site.network.answers = {fetched(ids.mid(0, 10)), fetched(ids.mid(10, 10)), fetched(ids.mid(20))};
        p.site.results.setSearch(foundBy(30, u"Ab3dE9"_s, 23));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 10);
        QVERIFY(QQuickTest::qWaitForPolish(p.window()));
        QVERIFY(shown(p.window(), u"30 listed in Runes of Aldur"_s));
        QVERIFY(!shown(p.window(), u"The trade site lists only the first 23. Open it to see the rest."_s));

        QVERIFY(click(p.window(), u"Load More"_s));
        QVERIFY(p.site.results.isBusy());
        QVERIFY(shown(p.window(), u"Asking the trade site for the listings"_s));
        // not again while it waits
        QVERIFY(!button(p.window(), u"Load More"_s)->isEnabled());
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 20);
        QVERIFY(!shown(p.window(), u"Asking the trade site for the listings"_s));

        QVERIFY(click(p.window(), u"Load More"_s));
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 23);
        QTRY_VERIFY(!button(p.window(), u"Load More"_s));
        QTRY_VERIFY(shown(p.window(), u"The trade site lists only the first 23. Open it to see the rest."_s));
        QCOMPARE(p.site.network.sent.size(), 3);
    }

    void opensTheTradeSite()
    {
        Browser browser;
        Panel p;
        p.site.network.answers = {fetched(listingIds(1))};
        const TradeFinder::Found found = foundBy(1);
        p.site.results.setSearch(found);
        p.panel.show();
        QVERIFY(p.loaded());
        QVERIFY(click(p.window(), u"Open on the Trade Site"_s));
        QCOMPARE(browser.opened, (QList<QUrl>{found.url}));
    }

    void noSearchNoLink()
    {
        // and it isn't opened by showing the results
        Browser browser;
        Panel p;
        p.panel.show();
        QVERIFY(browser.opened.isEmpty());
    }

    void closesAndOpens()
    {
        Panel p;
        p.site.network.answers = {fetched(listingIds(2))};
        p.site.results.setSearch(foundBy(2));
        p.panel.toggle();
        QVERIFY(p.panel.isVisible());
        QVERIFY(p.loaded());

        p.panel.toggle();
        QVERIFY(!p.panel.isVisible());
        p.panel.show();
        QVERIFY(p.panel.isVisible());
        // with the listings it had, which aren't asked for again
        QTRY_COMPARE(rows(p.window()), 2);
        QCOMPARE(p.site.network.sent.size(), 1);

        QVERIFY(click(p.window(), u"Close"_s));
        QVERIFY(!p.panel.isVisible());
    }

    void closeButton()
    {
        Panel p;
        p.panel.show();
        // an icon only, with its name for a tooltip
        QVERIFY(QQuickTest::qWaitForPolish(p.window()));
        QVERIFY(shown(p.window(), u"Close"_s, "QQuickAbstractButton"));
    }

    void newSearchWhileOpen()
    {
        Panel p;
        p.site.network.answers = {fetched(listingIds(2)), fetched({u"y1"_s})};
        p.site.results.setSearch(foundBy(2));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 2);

        // which it shows, as it's open
        p.site.results.setSearch(foundBy(1, u"Qw8rT2"_s, 1));
        QVERIFY(p.site.results.isBusy());
        QCOMPARE(p.site.network.sent.size(), 2);
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 1);
        QTRY_VERIFY(shown(p.window(), u"1 listed in Runes of Aldur"_s));
    }

    void newSearchWhileClosed()
    {
        // which is only asked for when you open it
        Panel p;
        p.site.network.answers = {fetched(listingIds(2)), fetched({u"y1"_s})};
        p.site.results.setSearch(foundBy(2));
        p.panel.show();
        QVERIFY(p.loaded());
        p.panel.close();

        p.site.results.setSearch(foundBy(1, u"Qw8rT2"_s, 1));
        QVERIFY(!p.site.results.isBusy());
        QCOMPARE(p.site.network.sent.size(), 1);
        p.panel.show();
        QVERIFY(p.site.results.isBusy());
        QVERIFY(p.loaded());
        QCOMPARE(p.site.network.sent.size(), 2);
        QTRY_COMPARE(rows(p.window()), 1);
    }

    void nothingListed()
    {
        Panel p;
        p.site.results.setSearch(foundBy(0));
        p.panel.show();
        QVERIFY(QQuickTest::qWaitForPolish(p.window()));
        QVERIFY(shown(p.window(), u"None listed in Runes of Aldur right now"_s));
        QVERIFY(!shown(p.window(), u"Check an item and search the trade site from the popup first."_s));
        QVERIFY(button(p.window(), u"Open on the Trade Site"_s));
        QVERIFY(!button(p.window(), u"Load More"_s));
        QVERIFY(p.site.network.sent.isEmpty());
    }

    void failedToLoad()
    {
        Panel p;
        p.site.network.answers = {{.error = QNetworkReply::HostNotFoundError}, fetched(listingIds(2))};
        p.site.results.setSearch(foundBy(2));
        p.panel.show();
        QVERIFY(p.loaded());
        const QString error = p.site.results.error();
        QVERIFY(!error.isEmpty());
        QCOMPARE(rows(p.window()), 0);
        QTRY_VERIFY(shown(p.window(), error));

        // for you to try again
        QVERIFY(click(p.window(), u"Try Again"_s));
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 2);
        QTRY_VERIFY(!shown(p.window(), error));
        QVERIFY(p.site.results.error().isEmpty());
        QVERIFY(!button(p.window(), u"Try Again"_s));
    }

    void tradeIsOff()
    {
        Panel p;
        p.site.results.setSearch(foundBy(2));
        p.site.searcher.setSettings(TradeSettings{.enabled = false, .league = {.name = league}});
        p.panel.show();
        QVERIFY(!p.site.results.isBusy());
        QCOMPARE(p.site.results.error(), u"Trade searches are turned off in the settings."_s);
        QTRY_VERIFY(shown(p.window(), p.site.results.error()));
        QVERIFY(p.site.network.sent.isEmpty());
    }

    void saysItWaits()
    {
        // as the site takes a request a second
        Panel p;
        FakeAnswer oneASecond = fetched(listingIds(10));
        oneASecond.headers = {{"X-Rate-Limit-Ip", "1:1:0"}};
        p.site.network.answers = {oneASecond, fetched(listingIds(20).mid(10))};
        p.site.results.setSearch(foundBy(25));
        p.panel.show();
        QVERIFY(p.loaded());

        // a request counts for a little over its second, so this leaves under one
        QTest::qWait(400);
        p.site.results.loadMore();
        QTRY_COMPARE(p.site.results.waiting(), 1);
        QTRY_VERIFY(shown(p.window(), u"Waiting a second for the trade site"_s));
        QVERIFY(!shown(p.window(), u"Asking the trade site for the listings"_s));
        QTRY_VERIFY_WITH_TIMEOUT(!p.site.results.isBusy(), 3000);
        QTRY_VERIFY(!shown(p.window(), u"Waiting a second for the trade site"_s));
        QTRY_COMPARE(rows(p.window()), 20);
    }

    void saysItWaitsLonger()
    {
        Panel p;
        FakeAnswer oneInFive = fetched(listingIds(10));
        oneInFive.headers = {{"X-Rate-Limit-Ip", "1:5:0"}};
        p.site.network.answers = {oneInFive};
        p.site.results.setSearch(foundBy(25));
        p.panel.show();
        QVERIFY(p.loaded());

        p.site.results.loadMore();
        QTRY_VERIFY(p.site.results.waiting() > 1);
        const QString wait = u"Waiting %1 seconds for the trade site"_s.arg(p.site.results.waiting());
        QTRY_VERIFY(shown(p.window(), wait));
    }

    void besideThePopup()
    {
        Panel p;
        p.panel.show();
        const QSize size = screen()->size();
        const QPoint corner = screen()->geometry().topLeft();
        QCOMPARE(p.window()->position(),
                 corner + placeResults(PopupPosition::Cursor, p.window()->size(), size));
        QCOMPARE(p.window()->x() + p.window()->width(), size.width() - 16);

        // and on the other side when the popup is on this one
        OverlaySettings settings;
        settings.position = PopupPosition::Right;
        p.panel.setSettings(settings);
        QCOMPARE(p.window()->position(),
                 corner + placeResults(PopupPosition::Right, p.window()->size(), size));
        QCOMPARE(p.window()->x(), 16);
    }

    void onTheScreenAskedFor()
    {
        // wherever that is, or the main one when it's not there
        Panel p;
        p.panel.show(u"HDMI-A-9"_s);
        QVERIFY(p.panel.isVisible());
        QCOMPARE(p.window()->screen(), screen());
        QCOMPARE(p.panel.maxHeight(), maxResultsHeight(screen()->size()));
    }

    void byTheMouse()
    {
        Panel p;
        p.input.mouseAt = ScreenPoint{.screen = screen()->name(), .position = {10, 10}};
        p.panel.show();
        QVERIFY(p.input.mouseLooks > 0);
        QCOMPARE(p.window()->screen(), screen());
    }

    void staysOnItsScreen()
    {
        Panel p;
        p.input.mouseAt = ScreenPoint{.screen = u"DP-1"_s, .position = {10, 10}};
        p.panel.show();
        QCOMPARE(p.panel.screenName(), u"DP-1"_s);

        // the mouse goes to another while it's open
        p.input.mouseAt = ScreenPoint{.screen = u"DP-2"_s, .position = {10, 10}};
        p.panel.show();
        QCOMPARE(p.panel.screenName(), u"DP-1"_s);

        // and it goes there once it's been closed
        p.panel.close();
        p.panel.show();
        QCOMPARE(p.panel.screenName(), u"DP-2"_s);

        // unless it's told, which it is even while it's open
        p.panel.show(u"DP-3"_s);
        QCOMPARE(p.panel.screenName(), u"DP-3"_s);
        p.panel.close();
        p.panel.show();
        QCOMPARE(p.panel.screenName(), u"DP-2"_s);
    }

    void growsToTheScreen()
    {
        // and scrolls from there
        Panel p;
        p.site.network.answers = {fetched(listingIds(10))};
        p.site.results.setSearch(foundBy(10));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 10);
        const int most = maxResultsHeight(screen()->size());
        QCOMPARE(p.panel.maxHeight(), most);
        QTRY_COMPARE(p.window()->height(), most);
        QCOMPARE(p.window()->position(),
                 screen()->geometry().topLeft() +
                     placeResults(PopupPosition::Cursor, p.window()->size(), screen()->size()));
    }

    void shrinksToTheListings()
    {
        Panel p;
        p.site.network.answers = {fetched(listingIds(1))};
        p.site.results.setSearch(foundBy(1));
        p.panel.show();
        const int empty = p.window()->height();
        QVERIFY(p.loaded());
        QTRY_VERIFY(p.window()->height() > empty);
        QCOMPARE_LT(p.window()->height(), maxResultsHeight(screen()->size()));
    }

    void scale()
    {
        Panel p;
        p.panel.show();
        const int width = p.window()->width();
        const int height = p.window()->height();
        OverlaySettings settings;
        settings.scale = 1.5;
        p.panel.setSettings(settings);
        QCOMPARE(p.panel.scale(), 1.5);
        QCOMPARE(p.window()->width(), qCeil(width / 1.0 * 1.5));
        QCOMPARE_GE(p.window()->height(), height);
        // the card is drawn that big, and the window has room for it
        const QList<QQuickItem *> inside = p.window()->contentItem()->childItems();
        QVERIFY(!inside.isEmpty());
        QCOMPARE(inside.first()->scale(), 1.5);
        QCOMPARE(p.window()->height(), qCeil(inside.first()->height() * 1.5));
    }

    void howEachDoes()
    {
        // for the character you play
        Panel p;
        p.site.results.judge().setCharacter(wearing(u"+30 to maximum Life"_s), std::nullopt);
        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        const QJsonObject listing = helmet(u"x1"_s, price(3, u"divine"_s), now);
        p.site.network.answers = {{.body = json({{u"result"_s, QJsonArray{listing}}})}};
        p.site.results.setSearch(foundBy(1));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 1);
        QVERIFY(QQuickTest::qWaitForPolish(p.window()));
        const QQuickItem *verdict = shown(p.window(), u"Upgrade"_s);
        QVERIFY(verdict);
        QCOMPARE(toneOf(verdict), int(Tone::Good));
        QVERIFY(shown(p.window(), u"+8 gear score"_s));
        const QQuickItem *life = shown(p.window(), u"Life +50"_s);
        QVERIFY(life);
        QCOMPARE(toneOf(life), int(Tone::Good));
        QVERIFY(!shown(p.window(), u"Checking with Path of Building"_s));
        // and no empty lines for what it doesn't do
        QVERIFY(!shown(p.window(), u""_s, "QQuickLabel"));
    }

    void whatItCosts()
    {
        // the cold res it takes off, and the fire res it leaves little over the cap
        Panel p;
        Character c = wearing(u"+30 to maximum Life\n+40% to Cold Resistance\n+10% to Fire Resistance"_s);
        ResistEntry entry;
        for (const Element e : allElements)
            entry.values[e] = 0.0;
        entry.values[Element::Cold] = 80.0;
        entry.values[Element::Fire] = 90.0;
        c.resists = snapshotResists(entry, c.gear, ResistSource::Manual);
        p.site.results.judge().setCharacter(c, std::nullopt);
        p.site.network.answers = {fetched(listingIds(1))};
        p.site.results.setSearch(foundBy(1));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 1);
        QVERIFY(QQuickTest::qWaitForPolish(p.window()));

        QCOMPARE(toneOf(shown(p.window(), u"Loses resistance"_s)), int(Tone::Bad));
        const QQuickItem *loss = shown(p.window(), u"Cold res drops below the cap (40% of 75%)"_s);
        QVERIFY(loss);
        QCOMPARE(toneOf(loss), int(Tone::Bad));
        const auto verdict = p.site.results.data(p.site.results.index(0), TradeResults::VerdictRole);
        const QString risk = verdict.value<ResultVerdict>().risks.value(0);
        QVERIFY(risk.startsWith(u"Fire res would be only 5% over the cap"));
        const QQuickItem *risky = shown(p.window(), risk);
        QVERIFY(risky);
        QCOMPARE(toneOf(risky), int(Tone::Warning));
    }

    void nothingToCompare()
    {
        // which leaves no gap where the verdict would be
        Panel p;
        TradeFinder::Found found = foundBy(1);
        found.item.itemClass = u"Stackable Currency"_s;
        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        const QJsonObject listing = helmet(u"x1"_s, price(3, u"divine"_s), now);
        p.site.network.answers = {{.body = json({{u"result"_s, QJsonArray{listing}}})}};
        p.site.results.setSearch(found);
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 1);
        QVERIFY(QQuickTest::qWaitForPolish(p.window()));
        QVERIFY(shown(p.window(), u"Grim Ward"_s));
        QVERIFY(!shown(p.window(), u""_s, "QQuickLabel"));
    }

    void whatItCaps()
    {
        // as Path of Building says
        FakePob pob;
        const QJsonObject capped{{u"FireResist"_s, 75}, {u"MissingFireResist"_s, 0}};
        const QJsonObject slot{{u"slot"_s, u"Helmet"_s},
                               {u"label"_s, u"Helmet"_s},
                               {u"replacing"_s, u"Grim Crown, Iron Hat"_s},
                               {u"stats"_s, capped}};
        const QJsonObject under{{u"FireResist"_s, 60}, {u"MissingFireResist"_s, 15}};
        QVERIFY(pob.answers(
            {{u"before"_s, under},
             {u"results"_s, QJsonArray{QJsonObject{{u"ok"_s, true}, {u"slots"_s, QJsonArray{slot}}}}}}));
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        Panel p;
        p.site.results.judge().setPob(&engine);
        p.site.results.judge().setCharacter(wearing(u"+30 to maximum Life"_s), PobBuild());
        p.site.network.answers = {fetched(listingIds(1))};
        p.site.results.setSearch(foundBy(1));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_VERIFY(shown(p.window(), u"Caps your fire resistance"_s));
        QCOMPARE(toneOf(shown(p.window(), u"Caps your fire resistance"_s)), int(Tone::Good));
    }

    void whilePathOfBuildingChecks()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install({u"--hang-on-eval"_s}));
        engine.setBuild(build);
        Panel p;
        p.site.results.judge().setPob(&engine);
        p.site.results.judge().setCharacter(wearing(u"+30 to maximum Life"_s), PobBuild());
        p.site.network.answers = {fetched(listingIds(1))};
        p.site.results.setSearch(foundBy(1));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 1);
        QTRY_VERIFY(shown(p.window(), u"Checking with Path of Building"_s));
    }

    void whenPathOfBuildingCant()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install({u"--crash-on-eval"_s}));
        engine.setBuild(build);
        Panel p;
        p.site.results.judge().setPob(&engine);
        p.site.results.judge().setCharacter(wearing(u"+30 to maximum Life"_s), PobBuild());
        p.site.network.answers = {fetched(listingIds(1))};
        p.site.results.setSearch(foundBy(1));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_COMPARE(rows(p.window()), 1);
        QTRY_VERIFY(shown(p.window(), u"Path of Building stopped unexpectedly."_s));
        QVERIFY(!shown(p.window(), u"Checking with Path of Building"_s));
        // and the gear score says what it can
        QVERIFY(shown(p.window(), u"+8 gear score"_s));
    }

    void colours()
    {
        Panel p;
        p.site.network.answers = {fetched(listingIds(1))};
        p.site.results.setSearch(foundBy(1));
        p.panel.show();
        QVERIFY(p.loaded());
        QTRY_VERIFY(shown(p.window(), u"Online"_s));
        const QColor online = shown(p.window(), u"Online"_s)->property("color").value<QColor>();
        QCOMPARE_NE(online, QColor(u"#3daee9"_s));

        OverlaySettings settings;
        settings.colorblind = true;
        p.panel.setSettings(settings);
        QCOMPARE(shown(p.window(), u"Online"_s)->property("color"), QColor(u"#3daee9"_s));
    }
};

QTEST_MAIN(TestResultsPanel)
#include "tst_resultspanel.moc"
