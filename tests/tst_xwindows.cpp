// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none, except for what doesn't need it.
#include "platform/xcb/xwindows.h"

#include <QCoreApplication>
#include <QRect>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

#define SKIP_WITHOUT_X()                                                                                     \
    if (!m_x.isValid())                                                                                      \
    QSKIP(qPrintable(m_x.errorString()))

class TestXWindows : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void describesAWindow()
    {
        SKIP_WITHOUT_X();
        const xcb_window_t window = createWindow();
        const char wmClass[] = "tst_xwindows\0TestXWindows";
        xcb_change_property(m_x.xcb(), XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
                            sizeof(wmClass), wmClass);
        const quint32 pid = QCoreApplication::applicationPid();
        xcb_change_property(m_x.xcb(), XCB_PROP_MODE_REPLACE, window, m_x.atom("_NET_WM_PID"),
                            XCB_ATOM_CARDINAL, 32, 1, &pid);

        const WindowInfo info = describeWindow(m_x, window);
        QCOMPARE(info.classes, (QStringList{u"tst_xwindows"_s, u"TestXWindows"_s}));
        QCOMPARE(info.program, u"tst_xwindows"_s);
        xcb_destroy_window(m_x.xcb(), window);
    }

    void saysNothing()
    {
        SKIP_WITHOUT_X();
        const xcb_window_t window = createWindow();
        WindowInfo info = describeWindow(m_x, window);
        QVERIFY(info.classes.isEmpty());
        QVERIFY(info.program.isEmpty());

        xcb_destroy_window(m_x.xcb(), window);
        info = describeWindow(m_x, window);
        QVERIFY(info.classes.isEmpty());
        QVERIFY(info.program.isEmpty());
    }

    void findsTheGame()
    {
        SKIP_WITHOUT_X();
        const xcb_window_t other = createWindow();
        const xcb_window_t game = createWindow();
        const char otherClass[] = "notepad\0Notepad";
        const char gameClass[] = "steam_app_2694490\0steam_app_2694490";
        xcb_change_property(m_x.xcb(), XCB_PROP_MODE_REPLACE, other, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
                            sizeof(otherClass), otherClass);
        xcb_change_property(m_x.xcb(), XCB_PROP_MODE_REPLACE, game, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
                            sizeof(gameClass), gameClass);
        QVERIFY(!anyGameWindow(m_x, {}));
        QVERIFY(!anyGameWindow(m_x, {other}));
        QVERIFY(anyGameWindow(m_x, {other, game}));
        xcb_destroy_window(m_x.xcb(), other);
        xcb_destroy_window(m_x.xcb(), game);
    }

    void pointer()
    {
        SKIP_WITHOUT_X();
        // wherever the mouse is, it's on the screen, over a program's window
        // or none
        const XReply<xcb_get_geometry_reply_t> root(
            xcb_get_geometry_reply(m_x.xcb(), xcb_get_geometry(m_x.xcb(), m_x.root()), nullptr));
        QVERIFY(root);
        const XPointer pointer = queryPointer(m_x);
        QVERIFY(pointer.position);
        QVERIFY(QRect(0, 0, root->width, root->height).contains(*pointer.position));
        const xcb_atom_t wmState = m_x.atom("WM_STATE");
        QVERIFY(pointer.window == XCB_WINDOW_NONE || !m_x.values(pointer.window, wmState, wmState).isEmpty());
    }

    void mouseOnScreens_data()
    {
        QTest::addColumn<std::optional<QPoint>>("position");
        QTest::addColumn<bool>("overWindow");
        QTest::addColumn<std::optional<ScreenPoint>>("expected");
        const auto at = [](int x, int y) {
            return std::optional<QPoint>(QPoint(x, y));
        };
        const auto on = [](const QString &screen, int x, int y) {
            return std::optional<ScreenPoint>(ScreenPoint{.screen = screen, .position = {x, y}});
        };

        // over the game's window, or under one that covers the screen, or a
        // Wayland window, where the X server has no window to name
        QTest::newRow("over a window") << at(5400, 1450) << true << on(u"DP-1"_s, 5400, 1450);
        QTest::newRow("over none") << at(5400, 1450) << false << on(u"DP-1"_s, 5400, 1450);
        QTest::newRow("on the second monitor") << at(8000, 100) << false << on(u"DP-2"_s, 320, 100);
        QTest::newRow("on no monitor") << at(9000, 2500) << false << std::optional<ScreenPoint>();
        QTest::newRow("nowhere") << std::optional<QPoint>() << false << std::optional<ScreenPoint>();
    }

    void mouseOnScreens()
    {
        QFETCH(std::optional<QPoint>, position);
        QFETCH(bool, overWindow);
        QFETCH(std::optional<ScreenPoint>, expected);

        const QList<NamedScreen> monitors{{.name = u"DP-1"_s, .geometry = QRect(0, 0, 7680, 2160)},
                                          {.name = u"DP-2"_s, .geometry = QRect(7680, 0, 1920, 1080)}};
        QCOMPARE(pointerOnScreens(
                     {.position = position, .window = xcb_window_t(overWindow ? 0x940001 : XCB_WINDOW_NONE)},
                     monitors, monitors),
                 expected);
    }

private:
    // one no one sees
    xcb_window_t createWindow()
    {
        const xcb_window_t window = xcb_generate_id(m_x.xcb());
        xcb_create_window(m_x.xcb(), XCB_COPY_FROM_PARENT, window, m_x.root(), 0, 0, 1, 1, 0,
                          XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
        return window;
    }

    XConnection m_x;
};

QTEST_GUILESS_MAIN(TestXWindows)
#include "tst_xwindows.moc"
