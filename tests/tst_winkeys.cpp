// SPDX-License-Identifier: MIT
#include "platform/win/winkeys.h"

#include <QKeySequence>
#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

constexpr quint32 alt = 0x1;
constexpr quint32 control = 0x2;
constexpr quint32 shift = 0x4;
constexpr quint32 win = 0x8;
constexpr quint32 noRepeat = 0x4000;

} // namespace

class TestWinKeys : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hotKey_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<quint32>("modifiers");
        QTest::addColumn<quint32>("code");

        QTest::newRow("letter") << u"Ctrl+D"_s << (control | noRepeat) << 0x44u;
        QTest::newRow("digit") << u"Alt+7"_s << (alt | noRepeat) << 0x37u;
        QTest::newRow("all the modifiers")
            << u"Ctrl+Alt+Shift+Meta+Q"_s << (control | alt | shift | win | noRepeat) << 0x51u;
        QTest::newRow("no modifiers") << u"F5"_s << noRepeat << 0x74u;
        QTest::newRow("last function key") << u"F24"_s << noRepeat << 0x87u;
        QTest::newRow("named") << u"Shift+Space"_s << (shift | noRepeat) << 0x20u;
        QTest::newRow("arrow") << u"Ctrl+Down"_s << (control | noRepeat) << 0x28u;
        QTest::newRow("punctuation") << u"Ctrl+["_s << (control | noRepeat) << 0xdbu;
        QTest::newRow("grave") << u"Ctrl+`"_s << (control | noRepeat) << 0xc0u;
    }

    void hotKey()
    {
        QFETCH(QString, key);
        QFETCH(quint32, modifiers);
        QFETCH(quint32, code);

        const std::optional<WinHotKey> hotKey = winHotKey(QKeySequence(key)[0]);
        QVERIFY(hotKey);
        QCOMPARE(hotKey->modifiers, modifiers);
        QCOMPARE(hotKey->key, code);
    }

    void keypad()
    {
        const auto keypad = [](Qt::Key key) {
            return winHotKey(QKeyCombination(Qt::ControlModifier | Qt::KeypadModifier, key));
        };
        QCOMPARE(keypad(Qt::Key_0), (WinHotKey{.modifiers = control | noRepeat, .key = 0x60}));
        QCOMPARE(keypad(Qt::Key_9), (WinHotKey{.modifiers = control | noRepeat, .key = 0x69}));
        QCOMPARE(keypad(Qt::Key_Minus), (WinHotKey{.modifiers = control | noRepeat, .key = 0x6d}));
        QCOMPARE(keypad(Qt::Key_Slash), (WinHotKey{.modifiers = control | noRepeat, .key = 0x6f}));
        // the main keyboard minus is a different key
        QCOMPARE(winHotKey(QKeyCombination(Qt::ControlModifier, Qt::Key_Minus))->key, 0xbdu);
    }

    void noCode()
    {
        QVERIFY(!winHotKey(QKeyCombination(Qt::ControlModifier, Qt::Key_Control)));
        QVERIFY(!winHotKey(QKeyCombination(Qt::ControlModifier, Qt::Key_unknown)));
        QVERIFY(!winHotKey(QKeyCombination(Qt::NoModifier, Qt::Key_VolumeUp)));
    }

    void modifiers()
    {
        QCOMPARE(winModifier(0xa2), Modifier::Control);
        QCOMPARE(winModifier(0xa3), Modifier::Control);
        QCOMPARE(winModifier(0xa4), Modifier::Alt);
        QCOMPARE(winModifier(0xa5), Modifier::Alt);
        QCOMPARE(winModifier(0xa0), Modifier::Other);
        QCOMPARE(winModifier(0xa1), Modifier::Other);
        QCOMPARE(winModifier(0x5b), Modifier::Other);
        QCOMPARE(winModifier(0x5c), Modifier::Other);
        QCOMPARE(winModifier(0x44), std::nullopt);
        QCOMPARE(winModifier(0x11), std::nullopt);
    }

    void countsAsHeld()
    {
        // the mouse buttons
        for (const quint32 key : {0x1u, 0x2u, 0x4u, 0x5u, 0x6u})
            QVERIFY2(!winCountsAsHeld(key), qPrintable(QString::number(key)));
        // Shift, Ctrl and Alt by themselves
        for (const quint32 key : {0x10u, 0x11u, 0x12u})
            QVERIFY2(!winCountsAsHeld(key), qPrintable(QString::number(key)));
        // Cancel, Backspace, the left Ctrl, D and F1
        for (const quint32 key : {0x3u, 0x8u, 0xa2u, 0x44u, 0x70u})
            QVERIFY2(winCountsAsHeld(key), qPrintable(QString::number(key)));
    }

    void copyKeys()
    {
        const CopyKeys keys = winCopyKeys();
        QCOMPARE(keys.control, 0xa2u);
        QCOMPARE(keys.alt, 0xa4u);
        QCOMPARE(keys.c, 0x43u);
    }
};

QTEST_GUILESS_MAIN(TestWinKeys)
#include "tst_winkeys.moc"
