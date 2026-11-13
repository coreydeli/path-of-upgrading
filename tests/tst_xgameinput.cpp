// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none, and when the game is open, as it would get the
// keys.
#include "platform/xcb/xgameinput.h"
#include "platform/xcb/xselectionwatcher.h"
#include "platform/xcb/xwindows.h"

#include <QScopeGuard>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTest>
#include <sys/socket.h>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestXGameInput : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void gameNotOpen_data()
    {
        QTest::addColumn<bool>("popupInTheWay");
        QTest::newRow("plain") << false;
        QTest::newRow("popup in the way") << true;
    }

    void gameNotOpen()
    {
        QFETCH(bool, popupInTheWay);
        XConnection x;
        if (!x.isValid())
            QSKIP(qPrintable(x.errorString()));
        for (const quint32 window : x.values(x.root(), x.atom("_NET_CLIENT_LIST"), XCB_ATOM_WINDOW)) {
            if (isGameWindow(describeWindow(x, window)))
                QSKIP("Path of Exile 2 is open, and it would get the keys.");
        }

        XGameInput input;
        QVERIFY2(input.isAvailable(), qPrintable(input.errorString()));
        QSignalSpy copied(&input, &GameInput::itemCopied);
        QSignalSpy failed(&input, &GameInput::copyFailed);
        input.copyItem({.nudge = popupInTheWay});
        if (popupInTheWay) {
            // which it waits to go from under the mouse, and the mouse isn't nudged
            // for a game that isn't there
            QVERIFY(failed.isEmpty());
            // another copy does nothing meanwhile
            input.copyItem({});
            QVERIFY(failed.isEmpty());
            QVERIFY(failed.wait());
        }
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.at(0).at(0).value<GameInput::Failure>(), GameInput::Failure::NotInFront);
        QVERIFY(copied.isEmpty());
    }

    void noDisplay()
    {
        const QByteArray display = qgetenv("DISPLAY");
        const auto putBack = qScopeGuard([&display] {
            if (!display.isNull())
                qputenv("DISPLAY", display);
        });
        qunsetenv("DISPLAY");

        // once on starting, and again when it tries for the copy
        const char *const warning = "Can't copy items out of the game: DISPLAY isn't set, so there's no X "
                                    "server or Xwayland to connect to.";
        QTest::ignoreMessage(QtWarningMsg, warning);
        QTest::ignoreMessage(QtWarningMsg, warning);
        XGameInput input;
        QVERIFY(!input.isAvailable());
        QCOMPARE(input.mouse(), std::nullopt);
        QSignalSpy failed(&input, &GameInput::copyFailed);
        input.copyItem({});
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.at(0).at(0).value<GameInput::Failure>(), GameInput::Failure::Unavailable);
        QCOMPARE(failed.at(0).at(1).toString(),
                 u"DISPLAY isn't set, so there's no X server or Xwayland to connect to."_s);
        QCOMPARE(failed.at(0).at(2).value<std::optional<ScreenPoint>>(), std::nullopt);
    }

    void keepsTryingAfterACopyWhileTheServerIsGone()
    {
        if (!XConnection().isValid())
            QSKIP("There's no X server.");
        XGameInput input;
        QVERIFY2(input.isAvailable(), qPrintable(input.errorString()));
        const auto *watcher = input.findChild<XSelectionWatcher *>();
        QVERIFY(watcher);
        const auto *notifier = watcher->findChild<QSocketNotifier *>();
        QVERIFY(notifier);
        QSignalSpy lost(watcher, &XSelectionWatcher::disconnected);
        // as if Xwayland had crashed, for this connection only
        QTest::ignoreMessage(QtWarningMsg, "Lost the X server, and will connect to it again in a while");
        QCOMPARE(::shutdown(int(notifier->socket()), SHUT_RDWR), 0);
        QVERIFY(lost.wait());

        // and isn't back yet when the hotkey is pressed
        const QByteArray display = qgetenv("DISPLAY");
        qunsetenv("DISPLAY");
        QTest::ignoreMessage(QtWarningMsg,
                             "Can't copy items out of the game: DISPLAY isn't set, so there's no X "
                             "server or Xwayland to connect to.");
        QSignalSpy failed(&input, &GameInput::copyFailed);
        input.copyItem({});
        qputenv("DISPLAY", display);
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.at(0).at(0).value<GameInput::Failure>(), GameInput::Failure::Unavailable);
        QVERIFY(!input.isAvailable());

        // it's back by the next try, with no other press
        QTRY_VERIFY_WITH_TIMEOUT(input.isAvailable(), 10000);
    }
};

QTEST_GUILESS_MAIN(TestXGameInput)
#include "tst_xgameinput.moc"
