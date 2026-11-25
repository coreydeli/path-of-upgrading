// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "app/fonts.h"
#include "core/parser.h"
#include "core/sessionguide.h"
#include "core/slots.h"
#include "core/weights.h"
#include "fakegameinput.h"
#include "fakegamewatcher.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "fakepob.h"
#include "fakesecretstore.h"
#include "itemtexts.h"
#include "mainwindowtest.h"
#include "services/datafiles.h"
#include "stylefortests.h"
#include "tradefixtures.h"
#include "updatetexts.h"

#include <KLocalizedQmlContext>
#include <QAccessible>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQuickTest/quicktest.h>
#include <functional>
#include <memory>
#include <optional>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using namespace pou::windowtest;

class TestMainWindowSettings : public QObject
{
    Q_OBJECT

public:
    // The window isn't for the screen. Kirigami would ask the desktop portal
    // for the look of the desktop, which starts it.
    static void initMain()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // the app's own controls, as the program has
        useStyleInTests();
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    }

private Q_SLOTS:
    void initTestCase()
    {
        // so the desktop's color settings are neither read nor written
        QStandardPaths::setTestModeEnabled(true);
        // a container can have no fonts at all, and the font pickers need one
        QVERIFY(addBundledFonts());
    }

    void init()
    {
        // Debian 13's Qt 6.8 now and then warns from its own animation timer that the
        // driver isn't running (Arch and Windows never do), so only that one passes there
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        QTest::failOnWarning(
            QRegularExpression(u"^(?!QUnifiedTimer::stopAnimationDriver: driver is not running$)"_s));
#else
        QTest::failOnWarning();
#endif
    }

    void generalName()
    {
        Window w;
        // the sidebar entry and the page both say General, apart from the Settings group they're in
        QCOMPARE(text(w.window->findChild<QObject *>(u"overlayAction"_s)), u"General"_s);
        QCOMPARE(text(w.window->findChild<QObject *>(u"tradeAction"_s)), u"Trade"_s);
        QCOMPARE(text(w.window->findChild<QObject *>(u"pobAction"_s)), u"Build Review"_s);
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"General"_s);
    }

    void buildReviewName()
    {
        Window w;
        // the sidebar entry and the page say Build Review, and the program keeps its own name inside
        QCOMPARE(text(w.window->findChild<QObject *>(u"pobAction"_s)), u"Build Review"_s);
        QVERIFY(w.show(u"pobAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Build Review"_s);
        QCOMPARE(text(w.find(u"analysisIntro"_s)),
                 u"Path of Building works out what an item would do to your build's damage and defences."_s);
        QCOMPARE(text(w.find(u"enabledSwitch"_s)), u"Judge items with Path of Building"_s);
    }

    void buildReviewTopsGearSettings()
    {
        Window w;
        QVERIFY(w.shown());
        // not a row of its own at the top, but the first in Gear Settings
        QTRY_VERIFY(w.sidebarRow(u"Gear Settings"_s));
        QVERIFY(!w.sidebarRow(u"Build Review"_s));
        QVERIFY(w.clickRow(u"Gear Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Build Review"_s));
        // the top level's rows linger while the group slides in, Your Gear where Build Review goes
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Your Gear"_s));
        const auto top = [&](const QString &text) {
            QQuickItem *row = w.sidebarRow(text);
            return row ? row->mapToScene(QPointF(0, 0)).y() : -1.0;
        };
        QVERIFY(top(u"Build Review"_s) > top(u"Back"_s));
        QVERIFY(top(u"Build Review"_s) < top(u"Characters"_s));
        // and the sidebar stays there while it's shown
        QVERIFY(w.clickRow(u"Build Review"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Build Review"_s);
        QTest::qWait(500);
        QVERIFY(w.sidebarRow(u"Back"_s));
        QCOMPARE(w.sidebarRow(u"Build Review"_s)->property("checked").toBool(), true);
    }

    void itemHistoryName()
    {
        Window w;
        // the sidebar entry
        QCOMPARE(text(w.window->findChild<QObject *>(u"checkedAction"_s)), u"Item History"_s);
        QVERIFY(w.show(u"checkedAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Item History"_s);
        // it says what it is, also before there is anything in it
        QObject *intro = w.find(u"historyIntro"_s);
        QVERIFY(isVisible(intro));
        QCOMPARE(text(intro),
                 u"Items you check in the game land here, newest first, up to the last "
                 "30. That covers the hotkey and, while \"Also check gear you copy yourself\" is "
                 "on, gear you copy by hand. When the list is full, the oldest one drops off."_s);
        QVERIFY(isVisible(w.find(u"noItemsText"_s)));
    }

    void checkedItems()
    {
        Window w;
        // checked while you were on another page
        w.app->history().add(item(itemtexts::helmetOld));
        w.app->history().add(item(itemtexts::advancedRing));
        QVERIFY(w.show(u"checkedAction"_s));
        QVERIFY(!isVisible(w.find(u"noItemsText"_s)));
        QVERIFY(isVisible(w.find(u"forgetSeparator"_s)));
        QVERIFY(isVisible(w.find(u"forgetButton"_s)));
        QVERIFY(isVisible(w.find(u"checkedHelp"_s)));

        // the newest first
        QObject *ring = w.find(u"checked:0"_s);
        QVERIFY(isVisible(ring));
        QCOMPARE(text(ring), u"Storm Loop"_s);
        QCOMPARE(ring->property("description").toString(), u"Rare, Sapphire Ring, Rings, item level 75"_s);
        QObject *again = ring->findChild<QObject *>(u"checkAgainButton"_s);
        QVERIFY(isVisible(again));
        // the buttons look alike, so each says which item it checks
        const QAccessibleInterface *reads = QAccessible::queryAccessibleInterface(again);
        QVERIFY(reads);
        QCOMPARE(reads->text(QAccessible::Name), u"Check Storm Loop Again"_s);
        QObject *helmet = w.find(u"checked:1"_s);
        QCOMPARE(text(helmet), u"Grim Crown"_s);
        QCOMPARE(helmet->property("description").toString(), u"Rare, Visored Helm, Helmets, item level 50"_s);
        QVERIFY(!search(w.page(), [](QObject *o) { return o->objectName() == u"checked:2"; }));

        // and one more at the front
        w.app->history().add(item(itemtexts::helmetNew));
        QCOMPARE(text(w.find(u"checked:0"_s)), u"Doom Visage"_s);
        QCOMPARE(text(w.find(u"checked:2"_s)), u"Grim Crown"_s);
    }

    void checkedItemLevels()
    {
        // an item without a level says what it needs, and a magic one has no base type
        Window w;
        QVERIFY(w.show(u"checkedAction"_s));
        w.app->history().add(item(u"Item Class: Boots\nRarity: Magic\nSturdy Leather Boots of the Fox\n"
                                  "--------\nRequirements:\nLevel: 12\n--------\n+10 to Dexterity"_s));
        QObject *boots = w.find(u"checked:0"_s);
        QCOMPARE(text(boots), u"Sturdy Leather Boots of the Fox"_s);
        QCOMPARE(boots->property("description").toString(), u"Magic, Boots, needs level 12"_s);

        w.app->history().add(item(u"Item Class: Belts\nRarity: Normal\nLeather Belt\n--------\n"
                                  "+5 to Strength"_s));
        QObject *belt = w.find(u"checked:0"_s);
        QCOMPARE(text(belt), u"Leather Belt"_s);
        QCOMPARE(belt->property("description").toString(), u"Normal, Belts"_s);
    }

    void checkAnItemAgain()
    {
        Window w;
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.show(u"checkedAction"_s));
        w.app->history().add(item(itemtexts::helmetNew));
        w.app->history().add(item(itemtexts::advancedRing));
        ItemPopup &popup = w.app->checker().popup();
        QVERIFY(!popup.isVisible());

        QVERIFY(click(w.find(u"checked:1"_s)->findChild<QObject *>(u"checkAgainButton"_s)));
        QVERIFY(popup.isVisible());
        QCOMPARE(popup.report().name, u"Doom Visage"_s);
        QCOMPARE(popup.report().versus, u"vs Grim Crown"_s);
        // which is the one you checked last now
        QCOMPARE(text(w.find(u"checked:0"_s)), u"Doom Visage"_s);
        QCOMPARE(text(w.find(u"checked:1"_s)), u"Storm Loop"_s);
        QCOMPARE(w.app->history().recent().size(), 2);
    }

    void forgetCheckedItems()
    {
        Window w;
        QVERIFY(w.show(u"checkedAction"_s));
        w.app->history().add(item(itemtexts::helmetOld));
        const SeenStats seen = w.app->history().seenStats();
        QVERIFY(!seen.isEmpty());

        QVERIFY(click(w.find(u"forgetButton"_s)));
        QObject *dialog = w.find(u"forgetDialog"_s);
        QVERIFY(isVisible(dialog));
        QCOMPARE(dialog->property("title").toString(), u"Forget the Items?"_s);
        // nothing yet
        QCOMPARE(w.app->history().recent().size(), 1);

        QVERIFY(click(button(dialog, u"Forget Them"_s)));
        QCOMPARE(w.app->history().recent().size(), 0);
        QVERIFY(isVisible(w.find(u"noItemsText"_s)));
        QVERIFY(!isVisible(w.find(u"forgetSeparator"_s)));
        QVERIFY(!isVisible(w.find(u"forgetButton"_s)));
        QVERIFY(!isVisible(w.find(u"checkedHelp"_s)));
        QTRY_VERIFY(!isVisible(dialog));
        QTRY_VERIFY(!search(w.page(), [](QObject *o) { return o->objectName() == u"checked:0"; }));
        // and the stat picker still has their stats
        QCOMPARE(w.app->history().seenStats(), seen);
    }

    void keepCheckedItems()
    {
        Window w;
        QVERIFY(w.show(u"checkedAction"_s));
        w.app->history().add(item(itemtexts::helmetOld));
        QVERIFY(click(w.find(u"forgetButton"_s)));
        QObject *dialog = w.find(u"forgetDialog"_s);
        QVERIFY(click(button(dialog, u"Cancel"_s)));
        QCOMPARE(w.app->history().recent().size(), 1);
        QVERIFY(!isVisible(w.find(u"noItemsText"_s)));
        QCOMPARE(text(w.find(u"checked:0"_s)), u"Grim Crown"_s);
        QTRY_VERIFY(!isVisible(dialog));
    }

    void league()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QObject *leagues = w.find(u"leagueBox"_s);
        // the current one, then the site's without their hardcore ones
        QCOMPARE(leagues->property("count").toInt(), 3);
        QCOMPARE(leagues->property("currentIndex").toInt(), 0);
        QCOMPARE(leagues->property("currentText").toString(), u"The current league"_s);
        QCOMPARE(w.find(u"searchedText"_s)->property("description").toString(), u"Runes of Aldur"_s);

        QVERIFY(activate(leagues, 2));
        QCOMPARE(w.settings().trade.league.name, u"Standard"_s);
        QVERIFY(toggle(w.find(u"hardcoreSwitch"_s)));
        QCOMPARE(w.settings().trade.league.hardcore, true);
        QCOMPARE(w.find(u"searchedText"_s)->property("description").toString(), u"Hardcore"_s);
    }

    void soloSelfFound()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.find(u"unavailableText"_s)->property("visible").toBool(), false);
        QVERIFY(toggle(w.find(u"soloSwitch"_s)));
        QCOMPARE(w.settings().trade.league.soloSelfFound, true);
        QObject *unavailable = w.find(u"unavailableText"_s);
        QCOMPARE(unavailable->property("visible").toBool(), true);
        QCOMPARE(text(unavailable), u"There's no trading in solo self-found, so trade searches are off."_s);
        QCOMPARE(w.find(u"searchedText"_s)->property("visible").toBool(), false);
        QVERIFY(!w.app->checker().popup().isTradeAvailable());
    }

    void showsTradeSettings()
    {
        Window w;
        QVERIFY(w.rated());
        Settings settings = w.settings();
        settings.trade.league = {.name = u"Standard"_s, .hardcore = true};
        settings.trade.status = ListingStatus::Online;
        settings.trade.budget.max = 3;
        QVERIFY(w.app->settings().set(settings));
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.find(u"leagueBox"_s)->property("currentText").toString(), u"Standard"_s);
        QCOMPARE(w.find(u"hardcoreSwitch"_s)->property("checked").toBool(), true);
        QCOMPARE(w.find(u"sellersBox"_s)->property("currentText").toString(), u"In person, while online"_s);
        QCOMPARE(w.spinBox(u"budgetMax"_s)->property("value").toInt(), 30);
    }

    void tradeOff()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(toggle(w.find(u"enabledSwitch"_s)));
        QCOMPARE(w.settings().trade.enabled, false);
        // and the page says why, and how to turn it on again
        QObject *unavailable = w.find(u"unavailableText"_s);
        QCOMPARE(unavailable->property("visible").toBool(), true);
        QCOMPARE(text(unavailable), tradeUnavailable({.enabled = false}));
        QVERIFY(text(unavailable).contains(u"Search the trade site"_s));
        QVERIFY(!w.app->checker().popup().isTradeAvailable());
        QCOMPARE(w.app->checker().popup().tradeNote(), text(unavailable));

        QVERIFY(toggle(w.find(u"enabledSwitch"_s)));
        QCOMPARE(w.find(u"unavailableText"_s)->property("visible").toBool(), false);
        QVERIFY(w.app->checker().popup().tradeNote().isEmpty());
    }

    void releaseSidebarHasNoTrade()
    {
        Window release(true, false, {}, used(), false);
        QVERIFY(release.window->findChild<QObject *>(u"tradeAction"_s));
        QVERIFY(!isVisible(release.window->findChild<QObject *>(u"tradeAction"_s)));
        QVERIFY(!release.app->tradeOffered());
        QVERIFY(release.settings().trade.enabled);
        QVERIFY(release.app->checker().popup().tradeNote().isEmpty());
        QVERIFY(!release.app->checker().popup().isTradeAvailable());
        QVERIFY(release.network->sent.isEmpty());
    }

    void betaSettingsHaveTheStartOverCard()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(isVisible(w.find(u"startOverCard"_s)));
        QVERIFY(isVisible(w.find(u"startOverButton"_s)));
    }

    void releaseSettingsHaveNoStartOverCard()
    {
        Window release(true, false, {}, used(), true, nullptr, true, true, false);
        QVERIFY(!release.app->startOver()->isOffered());
        QVERIFY(release.show(u"overlayAction"_s));
        QVERIFY(isVisible(release.find(u"checkKey"_s)));
        QVERIFY(!isVisible(release.find(u"startOverCard"_s)));
        QVERIFY(!isVisible(release.find(u"startOverButton"_s)));
    }

    void startOverAsksFirst()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        w.app->startOver()->setRestart([] { return true; });
        QObject *dialog = w.window->findChild<QObject *>(u"startOverDialog"_s);
        QVERIFY(dialog);
        QVERIFY(!dialog->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(w.find(u"startOverButton"_s), "clicked"));
        QVERIFY(QTest::qWaitFor([&] { return dialog->property("opened").toBool(); }, 5000));
        // nothing is wiped until it's said yes to
        QVERIFY(!w.app->startOver()->isBusy());
        QVERIFY(QFileInfo::exists(w.dir.filePath(u"settings.conf"_s)));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
    }

    void sidebarHasTradeWhenOffered()
    {
        Window w;
        QVERIFY(w.app->tradeOffered());
        QVERIFY(isVisible(w.window->findChild<QObject *>(u"tradeAction"_s)));
    }

    void releaseSettingsHaveNoResultsKey()
    {
        Window release(true, false, {}, used(), false);
        QVERIFY(release.show(u"overlayAction"_s));
        QVERIFY(isVisible(release.find(u"checkKey"_s)));
        QVERIFY(!isVisible(release.find(u"resultsKey"_s)));
        QVERIFY(!release.shortcuts->names.contains(u"trade-results"_s));
    }

    void releaseSetupGuideHasTheLeagueButNoTradeStep()
    {
        Window w(true, false, {}, std::nullopt, false);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        QCOMPARE(guide->property("steps").toStringList(),
                 (QStringList{u"welcome"_s, u"league"_s, u"character"_s, u"look"_s, u"done"_s}));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(guide->property("title").toString(), u"Pick Your League"_s);
        QVERIFY(isVisible(w.inGuide(u"leagueStep"_s)));
        // solo self-found, with or without hardcore, has nothing to turn off here
        QObject *solo = w.inGuide(u"setupSoloSwitch"_s);
        QCOMPARE(solo->property("description").toString(), u"For characters that play without trading."_s);
        QVERIFY(toggle(w.inGuide(u"setupHardcoreSwitch"_s)));
        QVERIFY(toggle(solo));
        QVERIFY(w.settings().trade.league.hardcore);
        QVERIFY(w.settings().trade.league.soloSelfFound);
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(guide->property("title").toString(), u"Your Character"_s);
        QVERIFY(!isVisible(w.inGuide(u"tradeStep"_s)));
        QVERIFY(w.network->sent.isEmpty());
    }

    void setupGuideSoloSelfFoundTurnsTradeOff()
    {
        Window w(true, false, {}, std::nullopt);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        // trade is on, which a first start only has in a beta build
        Settings on = w.settings();
        on.trade.enabled = true;
        QVERIFY(w.app->settings().set(on));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(w.app->trade().unavailable().isEmpty());
        QVERIFY(toggle(w.inGuide(u"setupHardcoreSwitch"_s)));
        QVERIFY(toggle(w.inGuide(u"setupSoloSwitch"_s)));
        QVERIFY(!w.app->trade().unavailable().isEmpty());
        QVERIFY(!w.app->checker().popup().isTradeAvailable());
        QCOMPARE(guide->property("steps").toStringList(),
                 (QStringList{u"welcome"_s, u"league"_s, u"character"_s, u"look"_s, u"done"_s}));
        QVERIFY(w.inGuide(u"setupSoloSwitch"_s)
                    ->property("description")
                    .toString()
                    .contains(u"turns trade searches off"_s));
    }

    void releaseSettingsHaveTheLeague()
    {
        Window w(true, false, {}, used(), false);
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(isVisible(w.find(u"leagueBox"_s)));
        QVERIFY(toggle(w.find(u"hardcoreSwitch"_s)));
        QVERIFY(toggle(w.find(u"soloSwitch"_s)));
        QVERIFY(w.settings().trade.league.hardcore);
        QVERIFY(w.settings().trade.league.soloSelfFound);
        // as the setup guide has them
        QVERIFY(w.network->sent.isEmpty());
    }

    void settingsLeagueIsOnTheTradePageWhenOffered()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(!isVisible(w.find(u"leagueBox"_s)));
    }

    void cardsInColumns()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.shown());
        const auto pos = [&](const QString &name) {
            const auto *item = qobject_cast<QQuickItem *>(w.find(name));
            return item ? item->mapToScene(QPointF(0, 0)) : QPointF(-1, -1);
        };

        // narrow, they're in one column, in the order they're written
        w.window->resize(700, 1400);
        QTRY_VERIFY(w.settled() && pos(u"leagueBox"_s).x() == pos(u"budgetMin"_s).x());
        QCOMPARE(pos(u"sellersBox"_s).x(), pos(u"leagueBox"_s).x());
        QCOMPARE(pos(u"currencyBox"_s).x(), pos(u"leagueBox"_s).x());
        QVERIFY(pos(u"leagueBox"_s).y() < pos(u"sellersBox"_s).y());
        QVERIFY(pos(u"sellersBox"_s).y() < pos(u"budgetMin"_s).y());
        QVERIFY(pos(u"budgetMin"_s).y() < pos(u"sessionField"_s).y());

        // wide, they're beside each other
        w.window->resize(1900, 900);
        QTRY_VERIFY(w.settled() && pos(u"leagueBox"_s).x() > pos(u"budgetMin"_s).x());
        QVERIFY(pos(u"sellersBox"_s).x() > pos(u"leagueBox"_s).x());
        QVERIFY(pos(u"sessionField"_s).x() > pos(u"leagueBox"_s).x());

        // and narrow again, they're back in the order they were written in
        w.window->resize(700, 1400);
        QTRY_VERIFY(w.settled() && pos(u"leagueBox"_s).x() == pos(u"budgetMin"_s).x());
        QVERIFY(pos(u"leagueBox"_s).y() < pos(u"sellersBox"_s).y());
        QVERIFY(pos(u"sellersBox"_s).y() < pos(u"budgetMin"_s).y());
        QVERIFY(pos(u"budgetMin"_s).y() < pos(u"sessionField"_s).y());
    }

    // Qt 6.8 warns if a drawer turns modal while it's open, so the sidebar closes first
    void sidebarClosesBeforeTurningModal()
    {
        Window w;
        QVERIFY(w.shown());
        const auto *drawer = w.window->property("globalDrawer").value<QObject *>();
        QVERIFY(drawer);
        const auto modal = [&] {
            return drawer->property("modal").toBool();
        };
        const auto open = [&] {
            return drawer->property("visible").toBool();
        };
        const auto settledOpen = [&] {
            return !modal() && open() && drawer->property("position").toDouble() == 1.0;
        };

        w.window->resize(1900, 900);
        QTRY_VERIFY(settledOpen());

        // it's shut by the time it's modal
        w.window->resize(450, 1400);
        QTRY_VERIFY(modal());
        QVERIFY(!open());

        w.window->resize(1900, 900);
        QTRY_VERIFY(settledOpen());

        // and a window that's wide again while the sidebar closes keeps it
        w.window->resize(450, 1400);
        QTRY_VERIFY(drawer->property("animating").toBool());
        w.window->resize(1900, 900);
        QTRY_VERIFY(settledOpen());
    }

    void budgetLabels()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.find(u"budgetMin"_s)->property("label").toString(), u"Lowest price"_s);
        QCOMPARE(w.find(u"budgetMax"_s)->property("label").toString(), u"Highest price"_s);
        QCOMPARE(w.find(u"budgetMin"_s)->property("description").toString(),
                 u"Anything cheaper won't show up."_s);
        QCOMPARE(w.find(u"budgetMax"_s)->property("description").toString(),
                 u"Anything pricier won't show up."_s);
        QCOMPARE(w.find(u"currencyBox"_s)->property("text").toString(), u"Currency"_s);
    }

    void ratePricesLabels()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QObject *autoRate = w.find(u"autoRateSwitch"_s);
        QCOMPARE(autoRate->property("text").toString(), u"Use poe.ninja's currency prices"_s);
        QCOMPARE(autoRate->property("description").toString(),
                 u"poe.ninja is a site that tracks what currency trades for in each league. Its price for a "
                 u"Divine Orb turns your budget into one currency. Right now a Divine Orb is 400 Exalted "
                 u"Orbs in Runes of Aldur."_s);
        QCOMPARE(w.find(u"rateMinutes"_s)->property("label").toString(),
                 u"Update poe.ninja's prices automatically"_s);
        QCOMPARE(w.find(u"rateMinutes"_s)->property("unit").toString(), u"minutes"_s);
        QVERIFY(w.settings().trade.rateMinutes > 1);
        QCOMPARE(w.find(u"divineRate"_s)->property("label").toString(), u"Exalted Orbs to a Divine Orb"_s);
        QCOMPARE(w.find(u"divineRate"_s)->property("description").toString(),
                 u"Used when poe.ninja's prices are off, or it has no price yet."_s);
    }

    void budget()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QObject *max = w.spinBox(u"budgetMax"_s);
        QCOMPARE(max->property("displayText").toString(), u"Any"_s);
        // in tenths
        QVERIFY(setValue(max, 25, "valueModified"));
        QCOMPARE(w.settings().trade.budget.max, 2.5);
        QCOMPARE(max->property("displayText").toString(), QLocale().toString(2.5, 'f', 1));
        QCOMPARE(max->property("stepSize").toInt(), 5);
        QCOMPARE(w.spinBox(u"budgetMin"_s)->property("stepSize").toInt(), 5);
        QCOMPARE(typed(max, QLocale().toString(1.5, 'f', 1)), 15);
        QCOMPARE(typed(max, u"Any"_s), 0);

        QVERIFY(activate(w.find(u"currencyBox"_s), 1));
        QCOMPARE(w.settings().trade.budget.currency, Currency::Exalted);
        QCOMPARE(max->property("stepSize").toInt(), 10);
        QVERIFY(setValue(w.spinBox(u"budgetMin"_s), 200, "valueModified"));
        QCOMPARE(w.settings().trade.budget.min, 20.0);
        QCOMPARE(w.spinBox(u"budgetMin"_s)->property("displayText").toString(), QLocale().toString(20));

        QVERIFY(activate(w.find(u"sellersBox"_s), 3));
        QCOMPARE(w.settings().trade.status, ListingStatus::Any);
    }

    void rate()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QObject *autoRate = w.find(u"autoRateSwitch"_s);
        QVERIFY(autoRate->property("description")
                    .toString()
                    .endsWith(u" Right now a Divine Orb is 400 Exalted Orbs in Runes of Aldur."_s));
        // which goes before yours
        QCOMPARE(w.find(u"divineRate"_s)->property("enabled").toBool(), false);

        // and is asked for again every ten minutes, as it says
        QObject *minutes = w.find(u"rateMinutes"_s);
        QCOMPARE(minutes->property("description").toString(),
                 u"Asks poe.ninja again after this many minutes, from 1 to 60. Prices updated just now."_s);
        QCOMPARE(w.spinBox(u"rateMinutes"_s)->property("value").toInt(), 10);
        QVERIFY(setValue(w.spinBox(u"rateMinutes"_s), 20, "valueModified"));
        QCOMPARE(w.settings().trade.rateMinutes, 20);
        QCOMPARE(minutes->property("unit").toString(), u"minutes"_s);
        QVERIFY(setValue(w.spinBox(u"rateMinutes"_s), 1, "valueModified"));
        QCOMPARE(minutes->property("unit").toString(), u"minute"_s);
        QVERIFY(setValue(w.spinBox(u"rateMinutes"_s), 20, "valueModified"));
        QCOMPARE(w.app->ratesRefresher().waitMinutes(), 20);
        QVERIFY(minutes->property("enabled").toBool());

        QVERIFY(toggle(autoRate));
        QCOMPARE(w.settings().trade.autoRate, false);
        // when it isn't, there's nothing to wait for
        QVERIFY(!minutes->property("enabled").toBool());
        QCOMPARE(minutes->property("description").toString(),
                 u"Asks poe.ninja again after this many minutes, from 1 to 60."_s);
        QCOMPARE(w.find(u"divineRate"_s)->property("enabled").toBool(), true);
        QVERIFY(setValue(w.spinBox(u"divineRate"_s), 3505, "valueModified"));
        QCOMPARE(w.settings().trade.budget.divineRate, 350.5);
    }

    void noRateYet()
    {
        // poe.ninja, which is asked as the app starts, and says nothing
        Window w(true, false, {}, used(), true, nullptr, true, false);
        QVERIFY(w.shown());
        QTRY_VERIFY(w.app->rates().isRefreshing());
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.find(u"autoRateSwitch"_s)
                    ->property("description")
                    .toString()
                    .endsWith(u" It has no price yet."_s));
        QCOMPARE(
            w.find(u"rateMinutes"_s)->property("description").toString(),
            u"Asks poe.ninja again after this many minutes, from 1 to 60. The prices haven't come in yet."_s);
        QCOMPARE(w.find(u"divineRate"_s)->property("enabled").toBool(), true);
    }

    void sessionNotSaved()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"No session ID is saved."_s);
        QVERIFY(!isVisible(w.find(u"sessionError"_s)));
        QVERIFY(text(w.find(u"sessionHelp"_s)).contains(u"system keyring"_s));
        // typed in as a password, with nothing to save or forget
        QCOMPARE(w.find(u"sessionField"_s)->property("echoMode").toInt(), 2);
        QVERIFY(!w.find(u"saveButton"_s)->property("enabled").toBool());
        QVERIFY(!w.find(u"forgetButton"_s)->property("enabled").toBool());
    }

    void findingTheSessionId()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QObject *steps = w.find(u"findSessionSteps"_s);
        QObject *button = w.find(u"findSessionButton"_s);
        QVERIFY(!isVisible(steps));
        QVERIFY(!isVisible(w.find(u"signInButton"_s)));
        QCOMPARE(text(button), u"Where Do I Find It?"_s);

        QVERIFY(click(button));
        QVERIFY(isVisible(steps));
        QCOMPARE(text(button), u"Hide Where to Find It"_s);
        QVERIFY(text(steps).startsWith(u"1. Sign in"_s));
        QVERIFY(text(steps).contains(u"POESESSID"_s));
        QVERIFY(steps->property("description").toString().contains(u"system keyring"_s));
        // nothing opens until you click for it
        QVERIFY(w.opener->opened.isEmpty());
        QVERIFY(click(w.find(u"signInButton"_s)));
        QCOMPARE(w.opener->opened, QList<QUrl>{signInUrl()});

        QVERIFY(click(button));
        QVERIFY(!isVisible(steps));
        QVERIFY(!isVisible(w.find(u"signInButton"_s)));
    }

    void saveSession()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QObject *field = w.find(u"sessionField"_s);
        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(w.find(u"saveButton"_s)->property("enabled").toBool());
        QVERIFY(click(w.find(u"saveButton"_s)));
        // while the keyring is asked
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"Waiting for the keyring."_s);
        QVERIFY(!field->property("enabled").toBool());
        QVERIFY(!w.find(u"saveButton"_s)->property("enabled").toBool());
        QVERIFY(w.keyringAnswered());

        QCOMPARE(w.keyring->secrets.value(u"POESESSID"_s), fakeId);
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"A session ID is saved."_s);
        QVERIFY(!isVisible(w.find(u"sessionError"_s)));
        // which isn't shown again
        QCOMPARE(text(field), QString());
        QVERIFY(field->property("enabled").toBool());
        QVERIFY(w.find(u"forgetButton"_s)->property("enabled").toBool());
    }

    void saveSessionWithEnter()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QObject *field = w.find(u"sessionField"_s);
        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(QMetaObject::invokeMethod(field, "accepted"));
        QVERIFY(w.keyringAnswered());
        QCOMPARE(w.keyring->secrets.value(u"POESESSID"_s), fakeId);
    }

    void notASessionId()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QObject *field = w.find(u"sessionField"_s);
        QVERIFY(field->setProperty("text", u"not an id!"_s));
        QVERIFY(click(w.find(u"saveButton"_s)));
        QObject *error = w.find(u"sessionError"_s);
        QVERIFY(isVisible(error));
        QCOMPARE(text(error), u"That doesn't look like a session ID. It's only letters and digits."_s);
        QCOMPARE(w.keyring->writes, 0);
        // which stays to be put right
        QCOMPARE(text(field), u"not an id!"_s);

        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(click(w.find(u"saveButton"_s)));
        QVERIFY(w.keyringAnswered());
        QVERIFY(!isVisible(error));
        QCOMPARE(w.keyring->secrets.value(u"POESESSID"_s), fakeId);
    }

    void keyringLocked()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        w.keyring->failure = u"The keyring is locked."_s;
        QObject *field = w.find(u"sessionField"_s);
        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(click(w.find(u"saveButton"_s)));
        QVERIFY(w.keyringAnswered());
        QObject *error = w.find(u"sessionError"_s);
        QVERIFY(isVisible(error));
        QCOMPARE(text(error), u"The keyring is locked."_s);
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"No session ID is saved."_s);
        // so that it can be tried again
        QCOMPARE(text(field), fakeId);
    }

    void forgetSession()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QVERIFY(w.find(u"sessionField"_s)->setProperty("text", fakeId));
        QVERIFY(click(w.find(u"saveButton"_s)));
        QVERIFY(w.keyringAnswered());
        QVERIFY(click(w.find(u"forgetButton"_s)));
        QVERIFY(w.keyringAnswered());
        QVERIFY(w.keyring->secrets.isEmpty());
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"No session ID is saved."_s);
        QVERIFY(!w.find(u"forgetButton"_s)->property("enabled").toBool());
        QVERIFY(!isVisible(w.find(u"sessionError"_s)));
    }

    void checkKey()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *recorder = w.recorder(u"checkKey"_s);
        QCOMPARE(recorder->property("keySequence").value<QKeySequence>(), QKeySequence(Qt::Key_F2));
        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F6)));
        QCOMPARE(w.shortcuts->keys.value(u"check-item"_s), QKeySequence(Qt::Key_F6));
        QCOMPARE(w.settings().overlay.hotkey, u"F6"_s);
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), false);
        QCOMPARE(w.find(u"hotkeysError"_s)->property("visible").toBool(), false);
    }

    void keyTaken()
    {
        Window w;
        w.shortcuts->taken << QKeySequence(Qt::Key_F4);
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *recorder = w.recorder(u"checkKey"_s);
        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F4)));
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), true);
        // it still has the one it had
        QCOMPARE(recorder->property("keySequence").value<QKeySequence>(), QKeySequence(Qt::Key_F2));
        QCOMPARE(w.settings().overlay.hotkey, u"F2"_s);

        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F6)));
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), false);
    }

    void resultsKey()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *recorder = w.recorder(u"resultsKey"_s);
        QCOMPARE(recorder->property("keySequence").value<QKeySequence>(), QKeySequence(u"Ctrl+Shift+D"_s));
        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F7)));
        QCOMPARE(w.shortcuts->keys.value(u"trade-results"_s), QKeySequence(Qt::Key_F7));
        QCOMPARE(w.settings().overlay.resultsHotkey, u"F7"_s);
        // and the other one stays where it was
        QCOMPARE(w.shortcuts->keys.value(u"check-item"_s), QKeySequence(Qt::Key_F2));
        QCOMPARE(w.settings().overlay.hotkey, u"F2"_s);
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), false);
    }

    void resultsKeyTaken()
    {
        // by the check key, as a key can only do the one
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *recorder = w.recorder(u"resultsKey"_s);
        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F2)));
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), true);
        QCOMPARE(recorder->property("keySequence").value<QKeySequence>(), QKeySequence(u"Ctrl+Shift+D"_s));
        QCOMPARE(w.settings().overlay.resultsHotkey, u"Ctrl+Shift+D"_s);
        QCOMPARE(w.shortcuts->keys.value(u"check-item"_s), QKeySequence(Qt::Key_F2));
    }

    void saysWhenTheKeysOnlyWorkInTheGame()
    {
        // one window at a time: on Debian's Qt 6.8, a second window made beside
        // the first now and then fails on Kirigami's drawer
        {
            Window always;
            QVERIFY(always.show(u"overlayAction"_s));
            QCOMPARE(always.find(u"gameOnlyNote"_s)->property("visible").toBool(), false);
        }

        auto *game = new FakeGameWatcher;
        Window w(true, false, {}, used(), true, game);
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.find(u"gameOnlyNote"_s)->property("visible").toBool(), true);
        QVERIFY(text(w.find(u"gameOnlyNote"_s)).contains(u"only work while Path of Exile 2 is running"_s));
    }

    void noHotkeys()
    {
        Window w(false);
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *error = w.find(u"hotkeysError"_s);
        QCOMPARE(error->property("visible").toBool(), true);
        QCOMPARE(text(error), w.app->overlayOptions()->hotkeysError());
    }

    void popupSettings()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *position = w.find(u"positionBox"_s);
        QCOMPARE(position->property("currentText").toString(), u"Next to the mouse"_s);
        QCOMPARE(w.spinBox(u"autoHide"_s)->property("value").toInt(), 60);
        QVERIFY(activate(position, 2));
        QCOMPARE(w.settings().overlay.position, PopupPosition::Right);

        QVERIFY(setValue(w.find(u"sizeSlider"_s), 1.5, "moved"));
        QCOMPARE(w.settings().overlay.scale, 1.5);
        QCOMPARE(text(w.find(u"sizeText"_s)), u"150%"_s);

        QVERIFY(setValue(w.spinBox(u"autoHide"_s), 30, "valueModified"));
        QCOMPARE(w.settings().overlay.autoHideSeconds, 30);

        QVERIFY(toggle(w.find(u"compactSwitch"_s)));
        QVERIFY(toggle(w.find(u"colorblindSwitch"_s)));
        QVERIFY(toggle(w.find(u"advancedCopySwitch"_s)));
        QVERIFY(toggle(w.find(u"putBackSwitch"_s)));
        QVERIFY(toggle(w.find(u"clipboardSwitch"_s)));
        QVERIFY(toggle(w.find(u"pausedSwitch"_s)));
        const OverlaySettings &overlay = w.settings().overlay;
        QCOMPARE(overlay.compact, false);
        QCOMPARE(overlay.colorblind, true);
        QCOMPARE(overlay.advancedCopy, true);
        QCOMPARE(overlay.putBackClipboard, false);
        QCOMPARE(overlay.watchClipboard, false);
        QCOMPARE(overlay.paused, true);
    }

    void startWithDesktop()
    {
#ifdef Q_OS_WIN
        QSKIP("On Windows it starts from the registry, which the tests keep out of");
#endif
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        // which it is until you turn it off
        QObject *start = w.find(u"startSwitch"_s);
        QCOMPARE(start->property("checked").toBool(), true);
        QVERIFY(QFile::exists(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QVERIFY(!isVisible(w.find(u"startError"_s)));
        QVERIFY(!isVisible(w.find(u"startErrorSeparator"_s)));

        QVERIFY(toggle(start));
        QVERIFY(!w.app->overlayOptions()->startsWithDesktop());
        QVERIFY(!QFile::exists(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QCOMPARE(start->property("checked").toBool(), false);

        QVERIFY(toggle(start));
        QVERIFY(QFile::exists(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QCOMPARE(start->property("checked").toBool(), true);
    }

    void startFailed()
    {
#ifdef Q_OS_WIN
        QSKIP("On Windows it starts from the registry, which the tests keep out of");
#endif
        Window w;
        // off, with a file where the folder should be
        w.app->overlayOptions()->setStartWithDesktop(false);
        QVERIFY(QDir(w.dir.filePath(u"autostart"_s)).removeRecursively());
        QFile blocker(w.dir.filePath(u"autostart"_s));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QVERIFY(w.show(u"overlayAction"_s));

        QObject *start = w.find(u"startSwitch"_s);
        QVERIFY(toggle(start));
        // which then goes back, and says why
        QCOMPARE(start->property("checked").toBool(), false);
        QObject *error = w.find(u"startError"_s);
        QVERIFY(isVisible(error));
        QVERIFY(isVisible(w.find(u"startErrorSeparator"_s)));
        QCOMPARE(text(error), w.app->overlayOptions()->startError());
        QVERIFY(!text(error).isEmpty());

        QVERIFY(QFile::remove(w.dir.filePath(u"autostart"_s)));
        QVERIFY(toggle(start));
        QCOMPARE(start->property("checked").toBool(), true);
        QVERIFY(!isVisible(error));
        QVERIFY(!isVisible(w.find(u"startErrorSeparator"_s)));
    }

    void startRemovedByHand()
    {
#ifdef Q_OS_WIN
        QSKIP("On Windows it starts from the registry, which the tests keep out of");
#endif
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(w.shown());
        QObject *start = w.find(u"startSwitch"_s);
        QCOMPARE(start->property("checked").toBool(), true);

        // while it's behind another window, which it then comes back from
        QWindow other;
        other.show();
        other.requestActivate();
        QTRY_VERIFY(other.isActive());
        QVERIFY(QFile::remove(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QCOMPARE(start->property("checked").toBool(), true);
        w.window->requestActivate();
        QTRY_VERIFY(w.window->isActive());
        QCOMPARE(start->property("checked").toBool(), false);
    }

    void showsPopupSettings()
    {
        Window w;
        Settings settings = w.settings();
        settings.overlay.position = PopupPosition::Top;
        settings.overlay.paused = true;
        QVERIFY(w.app->settings().set(settings));
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.find(u"positionBox"_s)->property("currentText").toString(), u"At the top of the screen"_s);
        QCOMPARE(w.find(u"pausedSwitch"_s)->property("checked").toBool(), true);
        // only as far as the popup goes
        QObject *autoHide = w.spinBox(u"autoHide"_s);
        QCOMPARE(autoHide->property("from").toInt(), 2);
        QCOMPARE(autoHide->property("to").toInt(), 120);
    }
};

QTEST_MAIN(TestMainWindowSettings)
#include "tst_mainwindow_settings.moc"
