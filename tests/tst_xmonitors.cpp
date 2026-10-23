// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none.
#include "platform/xcb/xmonitors.h"

#include <QTest>

using namespace pou;

class TestXMonitors : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        if (!m_x.isValid())
            QSKIP(qPrintable(m_x.errorString()));
    }

    void onTheScreen()
    {
        // whatever monitors there are, each has a name and is on the X screen
        const XReply<xcb_get_geometry_reply_t> root(
            xcb_get_geometry_reply(m_x.xcb(), xcb_get_geometry(m_x.xcb(), m_x.root()), nullptr));
        QVERIFY(root);
        const QRect screen(0, 0, root->width, root->height);
        const QList<NamedScreen> monitors = xMonitors(m_x);
        QVERIFY(!monitors.isEmpty());
        for (const NamedScreen &monitor : monitors) {
            QVERIFY(!monitor.name.isEmpty());
            QVERIFY(!monitor.geometry.isEmpty());
            QVERIFY(screen.contains(monitor.geometry));
        }
    }

private:
    XConnection m_x;
};

QTEST_GUILESS_MAIN(TestXMonitors)
#include "tst_xmonitors.moc"
