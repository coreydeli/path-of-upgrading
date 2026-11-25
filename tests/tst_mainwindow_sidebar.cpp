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
#include <QColor>
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

class TestMainWindowSidebar : public QObject
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

    // Build Review, Characters, Gear Score, Stat Weights and Ignored Stats are one level down, under Gear
    void gearPagesAreUnderGear()
    {
        Window w;
        QObject *group = w.window->findChild<QObject *>(u"gearGroupAction"_s);
        QVERIFY(group);
        QCOMPARE(group->property("text").toString(), u"Gear Settings"_s);
        for (const QString &name : {u"pobAction"_s, u"charactersAction"_s, u"gearScoreAction"_s,
                                    u"weightsAction"_s, u"ignoredAction"_s}) {
            QObject *action = w.window->findChild<QObject *>(name);
            QVERIFY2(action, qPrintable(name));
            QCOMPARE(action->parent(), group);
        }
        // Your Gear stays beside it
        QCOMPARE(w.window->findChild<QObject *>(u"gearAction"_s)->parent(), group->parent());

        // the group is lit while one of its pages is shown
        QCOMPARE(group->property("checked").toBool(), false);
        QVERIFY(w.show(u"weightsAction"_s));
        QCOMPARE(group->property("checked").toBool(), true);
        QVERIFY(w.show(u"gearAction"_s));
        QCOMPARE(group->property("checked").toBool(), false);
        QVERIFY(w.show(u"ignoredAction"_s));
        QCOMPARE(group->property("checked").toBool(), true);
        QVERIFY(w.show(u"gearScoreAction"_s));
        QCOMPARE(group->property("checked").toBool(), true);
        QVERIFY(w.show(u"charactersAction"_s));
        QCOMPARE(group->property("checked").toBool(), true);
        QVERIFY(w.show(u"pobAction"_s));
        QCOMPARE(group->property("checked").toBool(), true);
        QVERIFY(w.show(u"checkedAction"_s));
        QCOMPARE(group->property("checked").toBool(), false);
    }

    void sidebarOpensGear()
    {
        Window w;
        QVERIFY(w.shown());
        QCOMPARE(w.window->property("wideScreen").toBool(), true);
        // the pages aren't in the sidebar until Gear is opened
        QTRY_VERIFY(w.sidebarRow(u"Gear Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Your Gear"_s));
        QVERIFY(!w.sidebarRow(u"Gear Score"_s));
        QVERIFY(!w.sidebarRow(u"Stat Weights"_s));
        QVERIFY(!w.sidebarRow(u"Ignored Stats"_s));
        QVERIFY(!w.sidebarRow(u"Gear Score"_s));

        QVERIFY(w.clickRow(u"Gear Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Gear Score"_s));
        QVERIFY(w.sidebarRow(u"Characters"_s));
        QVERIFY(w.sidebarRow(u"Stat Weights"_s));
        QVERIFY(w.sidebarRow(u"Ignored Stats"_s));
        QVERIFY(w.sidebarRow(u"Back"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));
        // opening it shows no page yet
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);

        QVERIFY(w.clickRow(u"Stat Weights"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Stat Weights"_s);
        // the sidebar stays on Gear, so the next page is one click away
        QTest::qWait(500);
        QVERIFY(w.sidebarRow(u"Back"_s));
        QVERIFY(!w.sidebarRow(u"Item History"_s));
        QCOMPARE(w.sidebarRow(u"Stat Weights"_s)->property("checked").toBool(), true);
        QVERIFY(w.clickRow(u"Ignored Stats"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Ignored Stats"_s);
        QTest::qWait(500);
        QVERIFY(!w.sidebarRow(u"Item History"_s));
        QCOMPARE(w.sidebarRow(u"Ignored Stats"_s)->property("checked").toBool(), true);
        QCOMPARE(w.sidebarRow(u"Stat Weights"_s)->property("checked").toBool(), false);
        QVERIFY(w.clickRow(u"Gear Score"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Gear Score"_s);

        // Back goes up a level without picking a page
        QVERIFY(w.clickRow(u"Back"_s));
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Gear Score"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Gear Score"_s);
        QCOMPARE(w.sidebarRow(u"Gear Settings"_s)->property("checked").toBool(), true);
        QCOMPARE(w.sidebarRow(u"Item History"_s)->property("checked").toBool(), false);

        // a page from outside the group takes the sidebar back to the top
        QVERIFY(w.clickRow(u"Gear Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Back"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));
        QVERIFY(w.show(u"gearAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Back"_s));

        // the other pages stay one click away
        QVERIFY(w.clickRow(u"Your Gear"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QCOMPARE(w.sidebarRow(u"Gear Settings"_s)->property("checked").toBool(), false);
    }

    void sidebarGearWorksWithTheKeyboard()
    {
        Window w;
        QVERIFY(w.shown());
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
        w.sidebarRow(u"Your Gear"_s)->forceActiveFocus(Qt::TabFocusReason);
        QCOMPARE(w.focusedRow(), u"Your Gear"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Gear Settings"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Return);
        QTRY_VERIFY(w.sidebarRow(u"Gear Score"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));

        // Back, then the five pages
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Back"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Build Review"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Characters"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Gear Score"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Stat Weights"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Ignored Stats"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Space);
        QCOMPARE(w.page()->property("title").toString(), u"Ignored Stats"_s);

        // the sidebar is still on Gear, so another page is a key press away
        QTest::qWait(500);
        QVERIFY(!w.sidebarRow(u"Item History"_s));
        w.sidebarRow(u"Stat Weights"_s)->forceActiveFocus(Qt::TabFocusReason);
        QCOMPARE(w.focusedRow(), u"Stat Weights"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Return);
        QCOMPARE(w.page()->property("title").toString(), u"Stat Weights"_s);
    }

    // where the sidebar is a drawer you open
    void sidebarGearWorksInANarrowWindow()
    {
        Window w;
        w.window->resize(500, 700);
        w.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.window.get()));
        QVERIFY(w.settled());
        QTRY_COMPARE(w.window->property("wideScreen").toBool(), false);
        QTRY_COMPARE(w.drawer()->property("modal").toBool(), true);

        QVERIFY(QMetaObject::invokeMethod(w.drawer(), "open"));
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), true);
        QTRY_VERIFY(w.sidebarRow(u"Gear Settings"_s));
        QVERIFY(w.clickRow(u"Gear Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Gear Score"_s));
        // once it has slid in
        QTest::qWait(500);
        QVERIFY(w.clickRow(u"Gear Score"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Gear Score"_s);
        // the drawer lets go once a page is picked
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), false);

        // and opens on Gear again, so the next page is one click away
        QVERIFY(QMetaObject::invokeMethod(w.drawer(), "open"));
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), true);
        QTRY_VERIFY(w.sidebarRow(u"Stat Weights"_s));
        QVERIFY(!w.sidebarRow(u"Item History"_s));
        QTest::qWait(500);
        QVERIFY(w.clickRow(u"Stat Weights"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Stat Weights"_s);
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), false);

        // Back goes up a level, and a page from outside the group starts from the top
        QVERIFY(QMetaObject::invokeMethod(w.drawer(), "open"));
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), true);
        QTRY_VERIFY(w.sidebarRow(u"Back"_s));
        QVERIFY(w.clickRow(u"Back"_s));
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Stat Weights"_s));
        QCOMPARE(w.sidebarRow(u"Gear Settings"_s)->property("checked").toBool(), true);
        QVERIFY(w.clickRow(u"Your Gear"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), false);
        QVERIFY(QMetaObject::invokeMethod(w.drawer(), "open"));
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), true);
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
    }

    // General and Appearance are one level down, under Settings
    void settingsPagesAreUnderSettings()
    {
        Window w;
        QObject *group = w.window->findChild<QObject *>(u"settingsGroupAction"_s);
        QVERIFY(group);
        QCOMPARE(group->property("text").toString(), u"Settings"_s);
        for (const QString &name : {u"overlayAction"_s, u"appearanceAction"_s, u"updatesAction"_s,
                                    u"helpAction"_s, u"aboutAction"_s}) {
            QObject *action = w.window->findChild<QObject *>(name);
            QVERIFY2(action, qPrintable(name));
            QCOMPARE(action->parent(), group);
        }
        QCOMPARE(group->parent(), w.window->findChild<QObject *>(u"gearGroupAction"_s)->parent());

        QCOMPARE(group->property("checked").toBool(), false);
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(group->property("checked").toBool(), true);
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(group->property("checked").toBool(), true);
        QVERIFY(w.show(u"aboutAction"_s));
        QCOMPARE(group->property("checked").toBool(), true);
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(group->property("checked").toBool(), false);
    }

    void aboutPage()
    {
        Window w;
        QVERIFY(w.show(u"aboutAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"About"_s);
        // the version is the application's, and is empty in a test
        QCOMPARE(w.find(u"versionText"_s)->property("description").toString(),
                 u"unknown, a %1"_s.arg(isBetaBuild() ? u"beta"_s : u"release"_s));
        QCOMPARE(w.find(u"licenseText"_s)->property("description").toString(), u"MIT"_s);

        // nothing is opened until you click for it
        QVERIFY(w.opener->opened.isEmpty());
        QVERIFY(click(w.find(u"repositoryButton"_s)));
        QVERIFY(click(w.find(u"releasesButton"_s)));
        QVERIFY(click(w.find(u"issuesButton"_s)));
        QCOMPARE(w.opener->opened.size(), 3);
        QCOMPARE(w.opener->opened.first(), QUrl(u"https://github.com/coreydeli/path-of-upgrading"_s));
        QVERIFY(click(w.find(u"credit:Spectral"_s)));
        QCOMPARE(w.opener->opened.last(), QUrl(u"https://github.com/productiontype/Spectral"_s));
    }

    // the samples are drawn by the same functions the app draws with
    void colorKeyPage()
    {
        Window w(true, false, QStringLiteral(COLORS));
        QObject *action = w.window->findChild<QObject *>(u"colorKeyAction"_s);
        QVERIFY(action);
        QCOMPARE(action->property("text").toString(), u"Color Key"_s);
        // right after Item History
        QObject *checked = w.window->findChild<QObject *>(u"checkedAction"_s);
        QCOMPARE(action->parent(), checked->parent());
        QVERIFY(w.show(u"colorKeyAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Color Key"_s);

        const auto tint = [&](const QString &name) {
            QObject *row = w.find(name);
            QObject *sample = row ? row->findChild<QObject *>(u"sample"_s) : nullptr;
            return sample ? sample->property("color").value<QColor>() : QColor();
        };
        QCOMPARE(tint(u"rarity:Normal"_s), QColor(u"#c8c8c8"_s));
        QCOMPARE(tint(u"rarity:Magic"_s), QColor(u"#8888ff"_s));
        QCOMPARE(tint(u"rarity:Rare"_s), QColor(u"#ffff77"_s));
        QCOMPARE(tint(u"rarity:Unique"_s), QColor(u"#af6025"_s));
        QVERIFY(tint(u"grade:S"_s) != tint(u"grade:A"_s));
        QVERIFY(tint(u"grade:D"_s) != tint(u"grade:B"_s));
        QVERIFY(tint(u"mark:corrupted"_s) != tint(u"mark:fractured"_s));
        QVERIFY(w.find(u"frameSample"_s));

        // the colorblind setting changes better and worse
        const QColor good = tint(u"tone:good"_s);
        QVERIFY(good != tint(u"tone:bad"_s));
        w.app->overlayOptions()->setColorblind(true);
        QCOMPARE(tint(u"tone:good"_s), QColor(u"#3daee9"_s));
        QCOMPARE(tint(u"tone:bad"_s), QColor(u"#f67400"_s));
        QCOMPARE(tint(u"tone:warning"_s), QColor(u"#fdbc4b"_s));
        QVERIFY(tint(u"tone:good"_s) != good);

        // a sample says what it means, not only its color
        QObject *magic = w.find(u"rarity:Magic"_s)->findChild<QObject *>(u"sample"_s);
        QCOMPARE(spoken(magic), u"Magic"_s);
        QCOMPARE(spoken(w.find(u"tone:good"_s)->findChild<QObject *>(u"sample"_s)), u"Better"_s);
    }

    // chips are colored by their stat only in Pocket
    void colorKeyChipsFollowThePocketLook()
    {
        Window w(true, false, QStringLiteral(COLORS));
        QVERIFY(w.show(u"colorKeyAction"_s));
        QVERIFY(isVisible(w.find(u"chipsOneColor"_s)));
        const auto hasRow = [&] {
            return search(w.page(), [](QObject *o) { return o->objectName() == u"chipKey:resistance"_s; });
        };
        QVERIFY(!hasRow());

        w.app->appearance()->setDesign(u"pocket"_s);
        QTRY_VERIFY(hasRow());
        QVERIFY(!isVisible(w.find(u"chipsOneColor"_s)));
        QSet<QRgb> colors;
        for (const char *kind : {"resistance", "damage", "defence", "other"}) {
            QObject *row = w.find(u"chipKey:"_s + QString::fromLatin1(kind));
            QVERIFY(row);
            colors.insert(
                row->findChild<QObject *>(u"chipSample"_s)->property("tone").value<QColor>().rgba());
        }
        QCOMPARE(colors.size(), 4);

        w.app->appearance()->setDesign(u"dossier"_s);
        QTRY_VERIFY(isVisible(w.find(u"chipsOneColor"_s)));
    }

    void helpPage()
    {
        Window w;
        QVERIFY(w.show(u"helpAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Help"_s);

        // nothing is opened until you click for it
        QVERIFY(w.opener->opened.isEmpty());
        QVERIFY(click(w.find(u"wikiButton"_s)));
        QCOMPARE(w.opener->opened,
                 QList<QUrl>{QUrl(u"https://github.com/coreydeli/path-of-upgrading/wiki"_s)});
        QVERIFY(click(w.find(u"topic:Hotkeys-and-the-Overlay"_s)));
        QCOMPARE(w.opener->opened.last(),
                 QUrl(u"https://github.com/coreydeli/path-of-upgrading/wiki/Hotkeys-and-the-Overlay"_s));
    }

    void sidebarOpensSettings()
    {
        Window w;
        QVERIFY(w.shown());
        QTRY_VERIFY(w.sidebarRow(u"Settings"_s));
        QVERIFY(!w.sidebarRow(u"Appearance"_s));

        QVERIFY(w.clickRow(u"Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Appearance"_s));
        QVERIFY(w.sidebarRow(u"Back"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));
        // opening it shows no page yet
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);

        QVERIFY(w.clickRow(u"Appearance"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Appearance"_s);
        QTest::qWait(500);
        QVERIFY(w.sidebarRow(u"Back"_s));
        QVERIFY(!w.sidebarRow(u"Item History"_s));
        QCOMPARE(w.sidebarRow(u"Appearance"_s)->property("checked").toBool(), true);

        // Back goes up a level without picking a page
        QVERIFY(w.clickRow(u"Back"_s));
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Appearance"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Appearance"_s);
        QCOMPARE(w.sidebarRow(u"Settings"_s)->property("checked").toBool(), true);

        // a page from outside the group takes the sidebar back to the top
        QVERIFY(w.clickRow(u"Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Back"_s));
        QVERIFY(w.show(u"tradeAction"_s));
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Back"_s));
        QCOMPARE(w.sidebarRow(u"Settings"_s)->property("checked").toBool(), false);
    }

    // a button elsewhere that shows a page in another group, or from the top
    void sidebarFollowsPagesShownByCode()
    {
        Window w;
        QVERIFY(w.shown());
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));

        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"General"_s);
        QTRY_VERIFY(w.sidebarRow(u"Appearance"_s));
        QTRY_VERIFY(w.sidebarRow(u"Back"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));
        QTRY_COMPARE(w.sidebarRow(u"Appearance"_s)->property("checked").toBool(), false);

        // from a Gear page straight to a Settings page
        QVERIFY(w.show(u"weightsAction"_s));
        QTRY_VERIFY(w.sidebarRow(u"Stat Weights"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Appearance"_s));
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Appearance"_s);
        QTRY_VERIFY(w.sidebarRow(u"Appearance"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Stat Weights"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));
        QTRY_COMPARE(w.sidebarRow(u"Appearance"_s)->property("checked").toBool(), true);

        // and back to Gear
        QVERIFY(w.show(u"ignoredAction"_s));
        QTRY_VERIFY(w.sidebarRow(u"Ignored Stats"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Appearance"_s));
    }

    void sidebarSettingsWorksWithTheKeyboard()
    {
        Window w;
        QVERIFY(w.shown());
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
        w.sidebarRow(u"Settings"_s)->forceActiveFocus(Qt::TabFocusReason);
        QCOMPARE(w.focusedRow(), u"Settings"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Return);
        QTRY_VERIFY(w.sidebarRow(u"Appearance"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));

        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Back"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"General"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Tab);
        QCOMPARE(w.focusedRow(), u"Appearance"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Space);
        QCOMPARE(w.page()->property("title").toString(), u"Appearance"_s);

        // still on Settings, so the other page is a key press away
        QTest::qWait(500);
        QVERIFY(!w.sidebarRow(u"Item History"_s));
        w.sidebarRow(u"General"_s)->forceActiveFocus(Qt::TabFocusReason);
        QCOMPARE(w.focusedRow(), u"General"_s);
        QTest::keyClick(w.window.get(), Qt::Key_Return);
        QCOMPARE(w.page()->property("title").toString(), u"General"_s);
    }

    void sidebarSettingsWorksInANarrowWindow()
    {
        Window w;
        w.window->resize(500, 700);
        w.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.window.get()));
        QVERIFY(w.settled());
        QTRY_COMPARE(w.window->property("wideScreen").toBool(), false);
        QTRY_COMPARE(w.drawer()->property("modal").toBool(), true);

        QVERIFY(QMetaObject::invokeMethod(w.drawer(), "open"));
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), true);
        QTRY_VERIFY(w.sidebarRow(u"Settings"_s));
        QVERIFY(w.clickRow(u"Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Appearance"_s));
        QTest::qWait(500);
        QVERIFY(w.clickRow(u"Appearance"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Appearance"_s);
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), false);

        // it opens on Settings again
        QVERIFY(QMetaObject::invokeMethod(w.drawer(), "open"));
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), true);
        QTRY_VERIFY(w.sidebarRow(u"Appearance"_s));
        QVERIFY(!w.sidebarRow(u"Item History"_s));
        QTest::qWait(500);
        QVERIFY(w.clickRow(u"Back"_s));
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
        QTest::qWait(500);
        QVERIFY(w.clickRow(u"Your Gear"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), false);
        QVERIFY(QMetaObject::invokeMethod(w.drawer(), "open"));
        QTRY_COMPARE(w.drawer()->property("drawerOpen").toBool(), true);
        QTRY_VERIFY(w.sidebarRow(u"Item History"_s));
    }

    // the title and what the page is for, at the top of each page
    void pagesHaveHeaders()
    {
        Window w;
        const QStringList actions = {u"charactersAction"_s, u"gearAction"_s,       u"gearScoreAction"_s,
                                     u"weightsAction"_s,    u"ignoredAction"_s,    u"checkedAction"_s,
                                     u"colorKeyAction"_s,   u"pobAction"_s,        u"tradeAction"_s,
                                     u"overlayAction"_s,    u"appearanceAction"_s, u"updatesAction"_s,
                                     u"helpAction"_s,       u"aboutAction"_s};
        for (const QString &action : actions) {
            QVERIFY2(w.show(action), qPrintable(action));
            const QString title = w.page()->property("title").toString();
            QObject *header = w.find(u"pageHeader"_s);
            QVERIFY2(header, qPrintable(action));
            QCOMPARE(header->property("title").toString(), title);
            QVERIFY2(!header->property("description").toString().isEmpty(), qPrintable(title));
            QVERIFY(isVisible(header));
            QObject *line = header->findChild<QObject *>(u"pageDescription"_s);
            QVERIFY2(line, qPrintable(title));
            QVERIFY2(isVisible(line), qPrintable(title));
            QCOMPARE(text(line), header->property("description").toString());
        }
    }

    // The header laid itself out in a loop with some lengths of description, and the layout
    // warns when it does. So it tries lengths from none to a few lines, each in a window of its
    // own, at widths either side of every point the columns change at.
    void headerSettlesWithAnyDescription()
    {
        QString words;
        for (int i = 0; i < 40; ++i)
            words += u"some words that fill a line "_s;
        // the page's own first
        const QStringList descriptions = {QString(),
                                          u"Short."_s,
                                          u"Stats you don't want counted."_s,
                                          u"Stats your character does not want counted."_s,
                                          u"A line that is long enough to take most of a narrow window."_s,
                                          words.trimmed(),
                                          u""_s};
        for (const QString &description : descriptions) {
            Window w;
            QVERIFY(w.show(u"ignoredAction"_s));
            if (!description.isNull())
                QVERIFY(w.page()->setProperty("description", description));
            QVERIFY(w.shown());
            for (int width : {450, 700, 1000, 1040, 1100, 1500, 1560, 1900, 2200, 800}) {
                w.window->resize(width, 900);
                QVERIFY2(w.settled(), qPrintable(u"%1 wide"_s.arg(width)));
                const auto *header = qobject_cast<QQuickItem *>(w.find(u"pageHeader"_s));
                QVERIFY(header);
                QVERIFY(header->width() > 0);
                QVERIFY(header->width() <= w.page()->width());
                QObject *line = header->findChild<QObject *>(u"pageDescription"_s);
                QVERIFY(line);
                QCOMPARE(isVisible(line), !w.page()->property("description").toString().isEmpty());
            }
        }
    }

    void sidebarSaysWhoYouPlay()
    {
        Window w;
        const auto sidebar = [&](const QString &name) {
            return search(w.window.get(), [&](QObject *o) { return o->objectName() == name; });
        };
        QObject *who = sidebar(u"sidebarCharacter"_s);
        QVERIFY(who);
        QCOMPARE(text(who), u"No character yet"_s);

        w.play({{u"Helmet"_s, item(itemtexts::helmetNew)}});
        QCOMPARE(text(sidebar(u"sidebarCharacter"_s)), u"Wren"_s);
        QVERIFY(!w.app->gearList()->overall().isEmpty());
        // the gauge says the same score as the gear page does
        QObject *gauge = search(w.window.get(), [&](QObject *o) {
            return o->inherits("QQuickItem") && o->property("size").toString() == u"medium"_s &&
                   o->property("score").toString() == w.app->gearList()->overall();
        });
        QVERIFY(gauge);
        QCOMPARE(gauge->property("grade").toString(), w.app->gearList()->overallGrade());

        // and what it's built as, once it is
        QVERIFY(w.app->characterList()->currentBuild().isEmpty());
        QVERIFY(!isVisible(sidebar(u"sidebarBuild"_s)));
        const QString id = w.app->characterList()->current();
        const PobBuild build = parsePobBuild(ranger).value();
        QVERIFY(w.app->characterList()->importBuild(id, ranger, build));
        QCOMPARE(text(sidebar(u"sidebarBuild"_s)), u"Level 40 Ranger"_s);
        QVERIFY(isVisible(sidebar(u"sidebarBuild"_s)));
    }

    void charactersShowTheirGearScore()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        w.play({{u"Helmet"_s, item(itemtexts::helmetNew)}});
        QVERIFY(!w.app->characterList()->add(u"Ash"_s).isEmpty());
        // you play the one just added, which wears nothing
        QObject *ash = w.find(u"character:Ash"_s);
        QObject *wren = w.find(u"character:Wren"_s);
        QVERIFY(ash && wren);
        QObject *wrenScore = wren->findChild<QObject *>(u"rowScore"_s);
        QVERIFY(isVisible(wrenScore));
        QVERIFY(!isVisible(ash->findChild<QObject *>(u"rowScore"_s)));
        QCOMPARE(wren->property("detail").toString(),
                 u"A rough guide until Path of Building has weighed your build"_s);
        QCOMPARE(ash->property("detail").toString(), QString());
        QVERIFY(!wrenScore->property("score").toString().isEmpty());
        QCOMPARE(wrenScore->property("grade").toString(), gearGrade(wrenScore->property("score").toInt()));

        // the one you play has the accent frame
        const auto frame = [](QObject *row) {
            return search(row, [](QObject *o) { return o->objectName() == u"playedFrame"; });
        };
        QVERIFY(isVisible(frame(ash)));
        QVERIFY(!isVisible(frame(wren)));
        w.app->characterList()->setCurrent(wren->property("characterId").toString());
        QVERIFY(isVisible(frame(wren)));
        QVERIFY(!isVisible(frame(ash)));
    }
};

QTEST_MAIN(TestMainWindowSidebar)
#include "tst_mainwindow_sidebar.moc"
