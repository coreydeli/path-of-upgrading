// SPDX-License-Identifier: MIT
#include "app/desktop.h"
#include "app/hotkeys.h"
#include "app/overlayoptions.h"
#include "app/settingsstore.h"
#include "fakeglobalshortcuts.h"
#include "fakenotifier.h"
#include "faketray.h"
#include "platform/autostart.h"

#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// The desktop with a fake tray and notifier, and the window it shows.
struct Setup
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    FakeGlobalShortcuts shortcuts;
    Hotkeys hotkeys{&shortcuts};
    Autostart autostart{QString(), {}};
    OverlayOptions options{&settings, &hotkeys, &autostart};
    FakeTray tray;
    FakeNotifier notifier;
    QWindow window;
    Desktop desktop{&tray, &notifier, &options, &hotkeys};

    Setup() { hotkeys.add({}); }
};

} // namespace

class TestDesktop : public QObject
{
    Q_OBJECT

public:
    // The window isn't for the screen.
    static void initMain() { qputenv("QT_QPA_PLATFORM", "offscreen"); }

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void trayStartsWithTheSetting()
    {
        QTemporaryDir dir;
        SettingsStore settings(dir.filePath(u"settings.conf"_s));
        Settings paused;
        paused.overlay.paused = true;
        QVERIFY(settings.set(paused));
        FakeGlobalShortcuts shortcuts;
        Hotkeys hotkeys(&shortcuts);
        Autostart autostart(QString(), {});
        OverlayOptions options(&settings, &hotkeys, &autostart);
        FakeTray tray;
        FakeNotifier notifier;
        Desktop desktop(&tray, &notifier, &options, &hotkeys);
        QVERIFY(tray.paused);
    }

    void menuPausesTheHotkeys()
    {
        Setup s;
        QVERIFY(!s.options.isPaused());
        s.tray.choosePause(true);
        QVERIFY(s.options.isPaused());
        // and it's kept, as the window's switch does
        QVERIFY(s.settings.settings().overlay.paused);
        s.tray.choosePause(false);
        QVERIFY(!s.options.isPaused());
        QVERIFY(!s.settings.settings().overlay.paused);
    }

    void menuFollowsTheWindow()
    {
        Setup s;
        QVERIFY(!s.tray.paused);
        s.options.setPaused(true);
        QVERIFY(s.tray.paused);
        s.options.setPaused(false);
        QVERIFY(!s.tray.paused);
    }

    void trayShowsTheWindow()
    {
        Setup s;
        s.desktop.setWindow(&s.window);
        QCOMPARE(s.tray.window, &s.window);
        QVERIFY(!s.window.isVisible());
        s.tray.chooseShow();
        QVERIFY(s.window.isVisible());
    }

    void noWindowToShow()
    {
        Setup s;
        s.tray.chooseShow();
        s.desktop.showWindow();
        QVERIFY(!s.window.isVisible());

        // or one that's gone
        {
            QWindow gone;
            s.desktop.setWindow(&gone);
        }
        s.desktop.showWindow();
    }

    void startedAgain_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<bool>("shown");
        QTest::newRow("by hand") << QStringList{u"path-of-upgrading"_s} << true;
        QTest::newRow("from the launcher") << QStringList() << true;
        QTest::newRow("at login") << QStringList{u"path-of-upgrading"_s, u"--background"_s} << false;
        QTest::newRow("something else") << QStringList{u"path-of-upgrading"_s, u"--bogus"_s} << true;
    }

    void startedAgain()
    {
        QFETCH(QStringList, arguments);
        QFETCH(bool, shown);
        Setup s;
        s.desktop.setWindow(&s.window);
        s.desktop.startedAgain(arguments);
        QCOMPARE(s.window.isVisible(), shown);
    }

    void saysWhenTheHotkeysCantBeSet()
    {
        Setup s;
        s.shortcuts.available = false;
        s.desktop.reportProblems();
        QCOMPARE(s.notifier.sent.size(), 1);
        const FakeNotifier::Sent &sent = s.notifier.sent.first();
        QCOMPARE(sent.event, Notifier::Event::HotkeysUnavailable);
        QCOMPARE(sent.title, u"The Hotkeys Aren't Working"_s);
        QCOMPARE(sent.text, u"It's off. The settings are in the Path of Upgrading window."_s);
    }

    void saysNothingWhenTheyWork()
    {
        Setup s;
        s.desktop.reportProblems();
        QVERIFY(s.notifier.sent.isEmpty());
    }

    void staysQuietOtherwise()
    {
        Setup s;
        s.shortcuts.available = false;
        s.options.setPaused(true);
        s.tray.chooseShow();
        s.desktop.showWindow();
        s.desktop.startedAgain({});
        QVERIFY(s.notifier.sent.isEmpty());
    }
};

QTEST_MAIN(TestDesktop)
#include "tst_desktop.moc"
