// SPDX-License-Identifier: MIT
#include "app/popuphider.h"
#include "fakegameinput.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

namespace {

const QRect popup(1000, 400, 400, 300);

// Quick, for the tests, with time to see it didn't hide too soon.
PopupHider::Timeouts quick()
{
    return {.shown = 400ms, .left = 200ms, .away = 150ms, .look = 10ms};
}

// Long enough for it not to hide that way while the test looks at another.
PopupHider::Timeouts noShownTimeout()
{
    PopupHider::Timeouts t = quick();
    t.shown = 1h;
    return t;
}

ScreenPoint at(int x, int y, const QString &screen = u"DP-1"_s)
{
    return {.screen = screen, .position = {x, y}};
}

} // namespace

class TestPopupHider : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hidesAWhileAfterItsShown()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(100);
        QVERIFY(hide.isEmpty());
        QTRY_COMPARE(hide.size(), 1);
    }

    void startsAgainWhenShownAgain()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(250);
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(250);
        QVERIFY(hide.isEmpty());
        QTRY_COMPARE(hide.size(), 1);
    }

    void staysWhileHovered()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.shown(u"DP-1"_s, popup, true);
        hider.setHovered(true);
        // with the mouse far off as the game last saw it
        input.mouseAt = at(0, 0);
        QTest::qWait(500);
        QVERIFY(hide.isEmpty());

        // and goes sooner than it would have once it was shown
        hider.setHovered(false);
        input.mouseAt.reset();
        QTest::qWait(100);
        QVERIFY(hide.isEmpty());
        QTest::qWait(200);
        QCOMPARE(hide.size(), 1);
    }

    void notHoveredStill()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.shown(u"DP-1"_s, popup, true);
        hider.setHovered(false);
        QTest::qWait(300);
        QVERIFY(hide.isEmpty());
        QTRY_COMPARE(hide.size(), 1);
    }

    void hoveredAsItWent()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.shown(u"DP-1"_s, popup, true);
        hider.setHovered(true);
        hider.hidden();
        hider.shown(u"DP-1"_s, popup, true);
        QTRY_COMPARE(hide.size(), 1);
    }

    void hoveredBeforeItsShown()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.setHovered(true);
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(500);
        QVERIFY(hide.isEmpty());
    }

    void leavingWhenNotShown()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.setHovered(true);
        hider.setHovered(false);
        QTest::qWait(300);
        QVERIFY(hide.isEmpty());
    }

    void leavingAfterItWasHidden()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.shown(u"DP-1"_s, popup, true);
        hider.hidden();
        hider.setHovered(true);
        hider.setHovered(false);
        QTest::qWait(300);
        QVERIFY(hide.isEmpty());
    }

    void awayFromIt_data()
    {
        QTest::addColumn<ScreenPoint>("mouse");
        QTest::addColumn<bool>("hides");

        QTest::newRow("next to it") << at(1500, 500) << false;
        QTest::newRow("near its top left") << at(740, 140) << false;
        QTest::newRow("near its bottom right") << at(1659, 959) << false;
        QTest::newRow("past its right") << at(1661, 500) << true;
        QTest::newRow("past its left") << at(739, 500) << true;
        QTest::newRow("above it") << at(1200, 139) << true;
        QTest::newRow("below it") << at(1200, 961) << true;
        QTest::newRow("on another screen") << at(1200, 500, u"HDMI-A-1"_s) << true;
    }

    void awayFromIt()
    {
        QFETCH(ScreenPoint, mouse);
        QFETCH(bool, hides);

        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(noShownTimeout());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        input.mouseAt = mouse;
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(50);
        QVERIFY(hide.isEmpty());
        if (hides) {
            QTRY_COMPARE(hide.size(), 1);
        } else {
            QTest::qWait(300);
            QVERIFY(hide.isEmpty());
        }
    }

    void backBeforeLong()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        PopupHider::Timeouts timeouts = noShownTimeout();
        timeouts.away = 300ms;
        hider.setTimeouts(timeouts);
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.shown(u"DP-1"_s, popup, true);
        input.mouseAt = at(0, 0);
        QTest::qWait(200);
        input.mouseAt = at(1200, 500);
        QTest::qWait(100);
        input.mouseAt = at(0, 0);
        QTest::qWait(200);
        QVERIFY(hide.isEmpty());
        QTRY_COMPARE(hide.size(), 1);
    }

    void shownAgainWaitsForTheMouseAgain()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        PopupHider::Timeouts timeouts = noShownTimeout();
        timeouts.away = 300ms;
        hider.setTimeouts(timeouts);
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        input.mouseAt = at(0, 0);
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(200);
        // so the time the mouse has been away starts over for the new one
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(200);
        QVERIFY(hide.isEmpty());
        QTRY_COMPARE(hide.size(), 1);
    }

    void mouseUnknown()
    {
        // as over a window that isn't the game's
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(noShownTimeout());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(300);
        QVERIFY(hide.isEmpty());
    }

    void inTheWay_data()
    {
        QTest::addColumn<ScreenPoint>("mouse");
        QTest::addColumn<bool>("inTheWay");

        QTest::newRow("on it") << at(1200, 500) << true;
        QTest::newRow("on its top left corner") << at(1000, 400) << true;
        QTest::newRow("on its bottom right corner") << at(1399, 699) << true;
        QTest::newRow("just off its left") << at(996, 500) << true;
        QTest::newRow("just off its top") << at(1200, 396) << true;
        QTest::newRow("just off its right") << at(1403, 500) << true;
        QTest::newRow("just off its bottom") << at(1200, 703) << true;
        QTest::newRow("past its left") << at(995, 500) << false;
        QTest::newRow("past its top") << at(1200, 395) << false;
        QTest::newRow("past its right") << at(1404, 500) << false;
        QTest::newRow("past its bottom") << at(1200, 704) << false;
        QTest::newRow("on another screen") << at(1200, 500, u"HDMI-A-1"_s) << false;
    }

    void inTheWay()
    {
        QFETCH(ScreenPoint, mouse);
        QFETCH(bool, inTheWay);

        FakeGameInput input;
        PopupHider hider(&input);
        input.mouseAt = mouse;
        hider.shown(u"DP-1"_s, popup, true);
        QCOMPARE(hider.isInTheWay(), inTheWay);
    }

    void inTheWayWhenHovered()
    {
        // which is how it's known over the popup, where the mouse isn't
        FakeGameInput input;
        PopupHider hider(&input);
        hider.shown(u"DP-1"_s, popup, true);
        QVERIFY(!hider.isInTheWay());
        hider.setHovered(true);
        QVERIFY(hider.isInTheWay());
        input.mouseAt = at(0, 0);
        QVERIFY(hider.isInTheWay());
        hider.setHovered(false);
        QVERIFY(!hider.isInTheWay());
    }

    void notInTheWayWhenNotShown()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        input.mouseAt = at(1200, 500);
        hider.setHovered(true);
        QVERIFY(!hider.isInTheWay());

        hider.shown(u"DP-1"_s, popup, true);
        QVERIFY(hider.isInTheWay());
        hider.hidden();
        QVERIFY(!hider.isInTheWay());
        // not even with the mouse still where it was
        hider.setHovered(true);
        QVERIFY(!hider.isInTheWay());
    }

    void notByTheMouse()
    {
        // at the side of the screen, where you'd go past it to the item
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(noShownTimeout());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        input.mouseAt = at(0, 0);
        hider.shown(u"DP-1"_s, popup, false);
        QTest::qWait(300);
        QVERIFY(hide.isEmpty());

        hider.shown(u"DP-1"_s, popup, true);
        QTRY_COMPARE(hide.size(), 1);
    }

    // not all the time
    void looksAtTheMouseNowAndThen()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        // 10 times a second, as it is until it's told otherwise
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(250);
        QCOMPARE_LE(input.mouseLooks, 3);
    }

    void looksAsOftenAsItsTold()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        PopupHider::Timeouts timeouts = noShownTimeout();
        timeouts.look = 60ms;
        hider.setTimeouts(timeouts);
        hider.shown(u"DP-1"_s, popup, true);
        QTest::qWait(250);
        QCOMPARE_GT(input.mouseLooks, 0);
        QCOMPARE_LE(input.mouseLooks, 5);
    }

    void hiddenAnotherWay()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        input.mouseAt = at(0, 0);
        hider.shown(u"DP-1"_s, popup, true);
        hider.hidden();
        QTest::qWait(500);
        QVERIFY(hide.isEmpty());
    }

    void hidesOnce()
    {
        FakeGameInput input;
        PopupHider hider(&input);
        hider.setTimeouts(quick());
        QSignalSpy hide(&hider, &PopupHider::timeToHide);
        input.mouseAt = at(0, 0);
        hider.shown(u"DP-1"_s, popup, true);
        QTRY_COMPARE(hide.size(), 1);
        QTest::qWait(400);
        QCOMPARE(hide.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestPopupHider)
#include "tst_popuphider.moc"
