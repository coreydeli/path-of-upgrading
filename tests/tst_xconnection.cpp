// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none.
#include "platform/xcb/xconnection.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestXConnection : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        if (!m_x.isValid())
            QSKIP(qPrintable(m_x.errorString()));
    }

    void atoms()
    {
        const xcb_atom_t active = m_x.atom("_NET_ACTIVE_WINDOW");
        QVERIFY(active != XCB_ATOM_NONE);
        QCOMPARE(m_x.atom("_NET_ACTIVE_WINDOW"), active);
        QCOMPARE(m_x.atom("WM_CLASS"), xcb_atom_t(XCB_ATOM_WM_CLASS));
    }

    void properties()
    {
        // on a window no one sees
        const xcb_window_t window = xcb_generate_id(m_x.xcb());
        xcb_create_window(m_x.xcb(), XCB_COPY_FROM_PARENT, window, m_x.root(), 0, 0, 1, 1, 0,
                          XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
        const char wmClass[] = "tst_xconnection\0TestXConnection";
        xcb_change_property(m_x.xcb(), XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
                            sizeof(wmClass), wmClass);
        const xcb_atom_t numbers = m_x.atom("POU_TEST_NUMBERS");
        const quint32 values[] = {7, 61834, 4294967295u};
        xcb_change_property(m_x.xcb(), XCB_PROP_MODE_REPLACE, window, numbers, XCB_ATOM_CARDINAL, 32, 3,
                            values);

        QCOMPARE(m_x.bytes(window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING), QByteArray(wmClass, sizeof(wmClass)));
        QCOMPARE(m_x.bytes(window, XCB_ATOM_WM_CLASS, XCB_ATOM_ANY), QByteArray(wmClass, sizeof(wmClass)));
        QCOMPARE(m_x.values(window, numbers, XCB_ATOM_CARDINAL), (QList<quint32>{7, 61834, 4294967295u}));
        // asked for as another type, as numbers when it's bytes, or not set at all
        QVERIFY(m_x.values(window, numbers, XCB_ATOM_WINDOW).isEmpty());
        QVERIFY(m_x.values(window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING).isEmpty());
        QVERIFY(m_x.bytes(window, XCB_ATOM_WM_NAME, XCB_ATOM_ANY).isEmpty());

        xcb_destroy_window(m_x.xcb(), window);
        QVERIFY(m_x.bytes(window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING).isEmpty());
    }

    void lostConnection()
    {
        XConnection x;
        QVERIFY(x.isValid());
        QCOMPARE(x.errorString(), QString());

        // xcb gives up on the connection when a request is longer than the
        // server takes, the same as when the server goes away
        const xcb_window_t window = xcb_generate_id(x.xcb());
        xcb_create_window(x.xcb(), XCB_COPY_FROM_PARENT, window, x.root(), 0, 0, 1, 1, 0,
                          XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
        const QByteArray tooLong(qsizetype(xcb_get_maximum_request_length(x.xcb())) * 4, 'x');
        xcb_change_property(x.xcb(), XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                            tooLong.size(), tooLong.constData());

        QVERIFY(!x.isValid());
        QCOMPARE(x.errorString(), u"Lost the connection to the X server."_s);
    }

private:
    XConnection m_x;
};

QTEST_GUILESS_MAIN(TestXConnection)
#include "tst_xconnection.moc"
