// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none. Nothing gets pressed, since that would land in
// whatever window has the keyboard.
#include "platform/xcb/xkeysender.h"

#include <QTest>

using namespace pou;

class TestXKeySender : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        if (!m_sender.isValid())
            QSKIP(qPrintable(m_sender.errorString()));
    }

    void keysOnTheKeyboard()
    {
        const xcb_setup_t *setup = xcb_get_setup(m_x.xcb());
        const CopyKeys keys = m_sender.copyKeys();
        for (const quint32 key : {keys.control, keys.alt, keys.c}) {
            QVERIFY(key >= setup->min_keycode);
            QVERIFY(key <= setup->max_keycode);
        }
        QVERIFY(keys.control != keys.alt);
        QVERIFY(keys.control != keys.c);
    }

private:
    XConnection m_x;
    XKeySender m_sender;
};

QTEST_GUILESS_MAIN(TestXKeySender)
#include "tst_xkeysender.moc"
