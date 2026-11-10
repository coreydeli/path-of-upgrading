// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none. Whether a window is the game's is tested with the
// windows, in tst_xwindows.
#include "platform/xcb/xgamewatcher.h"

#include <QTest>

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
};

QTEST_GUILESS_MAIN(TestXGameWatcher)
#include "tst_xgamewatcher.moc"
