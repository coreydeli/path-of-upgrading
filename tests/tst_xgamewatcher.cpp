// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none. Whether a window is the game's is tested with the
// windows, in tst_xwindows.
#include "platform/xcb/xgamewatcher.h"

#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTest>
#include <sys/socket.h>

using namespace pou;

class TestXGameWatcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void watchesTheWindows()
    {
        XGameWatcher watcher;
        if (!watcher.isValid())
            QSKIP("There's no X server.");
        // it reads the windows and listens, whatever is open
        QTest::qWait(50);
        QVERIFY(watcher.isValid());
    }

    void connectsAgainAfterLosingTheServer()
    {
        XGameWatcher watcher;
        if (!watcher.isValid())
            QSKIP("There's no X server.");
        if (watcher.isRunning())
            QSKIP("Path of Exile 2 is open.");
        const auto *notifier = watcher.findChild<QSocketNotifier *>();
        QVERIFY(notifier);
        QSignalSpy changed(&watcher, &GameWatcher::runningChanged);
        // as if Xwayland had crashed, for this connection only
        QCOMPARE(::shutdown(int(notifier->socket()), SHUT_RDWR), 0);
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(changed.at(0).at(0).toBool(), true);
        QVERIFY(!watcher.isValid());
        // the server is still there, so it finds no game once it's back
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 2, 10000);
        QCOMPARE(changed.at(1).at(0).toBool(), false);
        QVERIFY(watcher.isValid());
    }
};

QTEST_GUILESS_MAIN(TestXGameWatcher)
#include "tst_xgamewatcher.moc"
