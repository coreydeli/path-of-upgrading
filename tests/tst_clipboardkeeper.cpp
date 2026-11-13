// SPDX-License-Identifier: MIT
#include "app/clipboardkeeper.h"
#include "fakeclipboard.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

class TestClipboardKeeper : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void putsBackWhatYouHad()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        clipboard.setData(u"text/html"_s, "<b>mine</b>");
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.keep();

        clipboard.copy(u"item"_s);
        keeper.putBack(u"item"_s);
        QTRY_COMPARE(clipboard.sets, 1);
        QCOMPARE(clipboard.text(), u"mine"_s);
        QCOMPARE(clipboard.data(u"text/html"_s), "<b>mine</b>");
    }

    // as on Wayland, where the game's copy comes in on X first
    void waitsForTheClipboardToHaveTheItem()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.keep();

        keeper.putBack(u"item"_s);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 0);
        clipboard.copy(u"item"_s);
        QTRY_COMPARE(clipboard.sets, 1);
        QCOMPARE(clipboard.text(), u"mine"_s);
    }

    void leavesWhatYouCopiedSince()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.keep();

        keeper.putBack(u"item"_s);
        clipboard.copy(u"something new"_s);
        // and the item after it is someone else's too
        clipboard.copy(u"item"_s);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 0);
        QCOMPARE(clipboard.text(), u"item"_s);
    }

    void leavesWhatYouCopiedBeforeItsPutBack()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.keep();

        clipboard.copy(u"item"_s);
        keeper.putBack(u"item"_s);
        // before it's had a chance
        clipboard.copy(u"something new"_s, false);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 0);
        QCOMPARE(clipboard.text(), u"something new"_s);
    }

    void givesUpWaiting()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.setWaitTime(10ms);
        keeper.keep();

        keeper.putBack(u"item"_s);
        QTest::qWait(50);
        clipboard.copy(u"item"_s);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 0);
    }

    void nothingToPutBack()
    {
        FakeClipboard clipboard;
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.keep();

        clipboard.copy(u"item"_s);
        keeper.putBack(u"item"_s);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 0);
        QCOMPARE(clipboard.text(), u"item"_s);
    }

    void notWhenOff()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.setEnabled(false);
        keeper.keep();
        QCOMPARE(clipboard.reads, 0);

        clipboard.copy(u"item"_s);
        keeper.putBack(u"item"_s);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 0);
    }

    void turnedOffWhileWaiting()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.keep();
        keeper.putBack(u"item"_s);

        keeper.setEnabled(false);
        clipboard.copy(u"item"_s);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 0);
    }

    void notWhenNothingWasCopied()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.keep();
        keeper.forget();

        clipboard.copy(u"item"_s);
        keeper.putBack(u"item"_s);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 0);
    }

    void onlyOnceForEachKeep()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        keeper.keep();
        clipboard.copy(u"item"_s);
        keeper.putBack(u"item"_s);
        QTRY_COMPARE(clipboard.sets, 1);

        clipboard.copy(u"item"_s);
        keeper.putBack(u"item"_s);
        QTest::qWait(20);
        QCOMPARE(clipboard.sets, 1);
    }

    void knowsWhatItPutBack()
    {
        FakeClipboard clipboard;
        clipboard.copy(u"mine"_s);
        ClipboardKeeper keeper;
        keeper.setClipboard(&clipboard);
        QVERIFY(!keeper.isPutBack(u"mine"_s));
        keeper.keep();
        clipboard.copy(u"item"_s);
        keeper.putBack(u"item"_s);
        QTRY_COMPARE(clipboard.sets, 1);

        QVERIFY(!keeper.isPutBack(u"item"_s));
        QVERIFY(keeper.isPutBack(u"mine"_s));
        // copied again, it's yours
        QVERIFY(!keeper.isPutBack(u"mine"_s));
    }

    void withoutAClipboard()
    {
        ClipboardKeeper keeper;
        keeper.keep();
        keeper.putBack(u"item"_s);
        QTest::qWait(20);
        QVERIFY(!keeper.isPutBack(u"item"_s));
    }
};

QTEST_MAIN(TestClipboardKeeper)
#include "tst_clipboardkeeper.moc"
