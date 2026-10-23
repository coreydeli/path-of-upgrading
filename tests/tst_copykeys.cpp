// SPDX-License-Identifier: MIT
#include "platform/copykeys.h"

#include <QTest>

using namespace pou;
using namespace std::chrono_literals;

namespace {

// the codes X has for them on a PC keyboard
constexpr CopyKeys keys = {.control = 37, .alt = 64, .c = 54};
constexpr quint32 leftShift = 50;
constexpr quint32 rightAlt = 108;

KeyStep down(quint32 key, std::chrono::milliseconds after)
{
    return {.key = key, .press = true, .after = after};
}

KeyStep up(quint32 key, std::chrono::milliseconds after)
{
    return {.key = key, .press = false, .after = after};
}

} // namespace

class TestCopyKeys : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void steps_data()
    {
        QTest::addColumn<QList<HeldModifier>>("held");
        QTest::addColumn<bool>("advanced");
        QTest::addColumn<QList<KeyStep>>("steps");

        QTest::newRow("copy") << QList<HeldModifier>() << false
                              << QList{down(keys.control, 0ms), down(keys.c, 15ms), up(keys.c, 40ms),
                                       up(keys.control, 15ms)};
        QTest::newRow("advanced copy")
            << QList<HeldModifier>() << true
            << QList{down(keys.control, 0ms), down(keys.alt, 15ms), down(keys.c, 15ms),
                     up(keys.c, 40ms),        up(keys.alt, 15ms),   up(keys.control, 15ms)};
        // Ctrl+Shift+D, with Ctrl left down for the user to let go of
        QTest::newRow("Ctrl and Shift held")
            << QList<HeldModifier>{{leftShift, Modifier::Other}, {keys.control, Modifier::Control}} << false
            << QList{up(leftShift, 0ms), down(keys.c, 15ms), up(keys.c, 40ms)};
        QTest::newRow("Alt held") << QList<HeldModifier>{{rightAlt, Modifier::Alt}} << false
                                  << QList{up(rightAlt, 0ms), down(keys.control, 0ms), down(keys.c, 15ms),
                                           up(keys.c, 40ms), up(keys.control, 15ms)};
        QTest::newRow("Alt held, advanced copy")
            << QList<HeldModifier>{{rightAlt, Modifier::Alt}} << true
            << QList{down(keys.control, 0ms), down(keys.c, 15ms), up(keys.c, 40ms), up(keys.control, 15ms)};
    }

    void steps()
    {
        QFETCH(QList<HeldModifier>, held);
        QFETCH(bool, advanced);
        QFETCH(QList<KeyStep>, steps);
        QCOMPARE(copySteps(keys, held, advanced), steps);
    }
};

QTEST_GUILESS_MAIN(TestCopyKeys)
#include "tst_copykeys.moc"
