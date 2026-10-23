// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none.
#include "platform/xcb/xwindows.h"

#include <QCoreApplication>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestXWindows : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        if (!m_x.isValid())
            QSKIP(qPrintable(m_x.errorString()));
    }

    void describesAWindow()
    {
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
        const xcb_window_t window = createWindow();
        WindowInfo info = describeWindow(m_x, window);
        QVERIFY(info.classes.isEmpty());
        QVERIFY(info.program.isEmpty());

        xcb_destroy_window(m_x.xcb(), window);
        info = describeWindow(m_x, window);
        QVERIFY(info.classes.isEmpty());
        QVERIFY(info.program.isEmpty());
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
