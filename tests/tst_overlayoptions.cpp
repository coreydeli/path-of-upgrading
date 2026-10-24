// SPDX-License-Identifier: MIT
#include "app/hotkeys.h"
#include "app/overlayoptions.h"
#include "app/settingsstore.h"
#include "fakeglobalshortcuts.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <limits>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// The settings, and the hotkeys with the keys they start with.
struct Options
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    FakeGlobalShortcuts shortcuts;
    Hotkeys hotkeys{&shortcuts};
    OverlayOptions options{&settings, &hotkeys};

    Options() { hotkeys.add({}); }

    const OverlaySettings &overlay() const { return settings.settings().overlay; }
};

} // namespace

class TestOverlayOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void readsTheSettings()
    {
        Options o;
        Settings settings;
        OverlaySettings &overlay = settings.overlay;
        overlay.advancedCopy = true;
        overlay.watchClipboard = false;
        overlay.position = PopupPosition::Right;
        overlay.scale = 1.5;
        overlay.compact = false;
        overlay.colorblind = true;
        overlay.autoHideSeconds = 30;
        overlay.paused = true;
        QVERIFY(o.settings.set(settings));

        QCOMPARE(o.options.isAdvancedCopy(), true);
        QCOMPARE(o.options.isWatchingClipboard(), false);
        QCOMPARE(o.options.position(), u"right"_s);
        QCOMPARE(o.options.scale(), 1.5);
        QCOMPARE(o.options.isCompact(), false);
        QCOMPARE(o.options.isColorblind(), true);
        QCOMPARE(o.options.autoHideSeconds(), 30);
        QCOMPARE(o.options.isPaused(), true);
    }

    void savesWhatYouChange()
    {
        Options o;
        QSignalSpy changed(&o.options, &OverlayOptions::changed);
        o.options.setAdvancedCopy(true);
        o.options.setWatchClipboard(false);
        o.options.setPosition(u"top"_s);
        o.options.setScale(0.75);
        o.options.setCompact(false);
        o.options.setColorblind(true);
        o.options.setAutoHideSeconds(45);
        o.options.setPaused(true);
        QCOMPARE(changed.size(), 8);

        OverlaySettings overlay;
        overlay.advancedCopy = true;
        overlay.watchClipboard = false;
        overlay.position = PopupPosition::Top;
        overlay.scale = 0.75;
        overlay.compact = false;
        overlay.colorblind = true;
        overlay.autoHideSeconds = 45;
        overlay.paused = true;
        QCOMPARE(o.overlay(), overlay);
        QCOMPARE(SettingsStore(o.dir.filePath(u"settings.conf"_s)).settings().overlay, overlay);

        o.options.setColorblind(true);
        QCOMPARE(changed.size(), 8);
    }

    void andBack()
    {
        Options o;
        o.options.setAdvancedCopy(true);
        o.options.setWatchClipboard(false);
        o.options.setPosition(u"left"_s);
        o.options.setCompact(false);
        o.options.setColorblind(true);
        o.options.setPaused(true);
        o.options.setAdvancedCopy(false);
        o.options.setWatchClipboard(true);
        o.options.setPosition(u"cursor"_s);
        o.options.setCompact(true);
        o.options.setColorblind(false);
        o.options.setPaused(false);
        QCOMPARE(o.overlay(), OverlaySettings());
    }

    void unknownPosition()
    {
        Options o;
        o.options.setPosition(u"right"_s);
        o.options.setPosition(u"middle"_s);
        QCOMPARE(o.overlay().position, PopupPosition::Cursor);
    }

    void scale_data()
    {
        QTest::addColumn<double>("typed");
        QTest::addColumn<double>("kept");
        QTest::newRow("some") << 1.25 << 1.25;
        QTest::newRow("smallest") << 0.5 << 0.5;
        QTest::newRow("too small") << 0.1 << 0.5;
        QTest::newRow("biggest") << 2.0 << 2.0;
        QTest::newRow("too big") << 5.0 << 2.0;
        QTest::newRow("not a number") << std::numeric_limits<double>::quiet_NaN() << 1.5;
    }

    void scale()
    {
        QFETCH(double, typed);
        QFETCH(double, kept);
        Options o;
        o.options.setScale(1.5);
        o.options.setScale(typed);
        QCOMPARE(o.overlay().scale, kept);
    }

    void autoHide_data()
    {
        QTest::addColumn<int>("typed");
        QTest::addColumn<int>("kept");
        QTest::newRow("some") << 20 << 20;
        QTest::newRow("shortest") << 2 << 2;
        QTest::newRow("too short") << 0 << 2;
        QTest::newRow("longest") << 120 << 120;
        QTest::newRow("too long") << 600 << 120;
    }

    void autoHide()
    {
        QFETCH(int, typed);
        QFETCH(int, kept);
        Options o;
        o.options.setAutoHideSeconds(typed);
        QCOMPARE(o.overlay().autoHideSeconds, kept);
    }

    void hotkeys()
    {
        Options o;
        QCOMPARE(o.options.checkKey(), QKeySequence(Qt::Key_F2));
        QCOMPARE(o.options.resultsKey(), QKeySequence(u"Ctrl+Shift+D"_s));
        QVERIFY(o.options.hotkeysError().isEmpty());

        QSignalSpy changed(&o.options, &OverlayOptions::changed);
        QVERIFY(o.options.setCheckKey(QKeySequence(Qt::Key_F3)));
        QVERIFY(o.options.setResultsKey(QKeySequence(u"Alt+D"_s)));
        QCOMPARE(changed.size(), 2);
        QCOMPARE(o.options.checkKey(), QKeySequence(Qt::Key_F3));
        QCOMPARE(o.options.resultsKey(), QKeySequence(u"Alt+D"_s));
        QCOMPARE(o.shortcuts.keys.value(u"check-item"_s), QKeySequence(Qt::Key_F3));

        // none
        QVERIFY(o.options.setResultsKey({}));
        QCOMPARE(o.options.resultsKey(), QKeySequence());
    }

    void keyTaken()
    {
        Options o;
        o.shortcuts.taken << QKeySequence(Qt::Key_F4);
        QVERIFY(!o.options.setCheckKey(QKeySequence(Qt::Key_F4)));
        // nor the other hotkey's
        QVERIFY(!o.options.setResultsKey(QKeySequence(Qt::Key_F2)));
        QCOMPARE(o.options.checkKey(), QKeySequence(Qt::Key_F2));
        QCOMPARE(o.options.resultsKey(), QKeySequence(u"Ctrl+Shift+D"_s));
    }

    void changedOnTheDesktop()
    {
        Options o;
        QSignalSpy changed(&o.options, &OverlayOptions::changed);
        o.shortcuts.change(u"trade-results"_s, QKeySequence(u"Meta+D"_s));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(o.options.resultsKey(), QKeySequence(u"Meta+D"_s));
    }

    void noHotkeys()
    {
        Options o;
        o.shortcuts.available = false;
        QCOMPARE(o.options.hotkeysError(), u"It's off."_s);
        QCOMPARE(o.options.checkKey(), QKeySequence());
    }
};

QTEST_GUILESS_MAIN(TestOverlayOptions)
#include "tst_overlayoptions.moc"
