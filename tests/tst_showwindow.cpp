// SPDX-License-Identifier: MIT
#include "platform/showwindow.h"

#include <QGuiApplication>
#include <QTest>
#include <QWindow>

using namespace pou;

class TestShowWindow : public QObject
{
    Q_OBJECT

public:
    // The windows aren't for the screen.
    static void initMain() { qputenv("QT_QPA_PLATFORM", "offscreen"); }

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void showsAHiddenWindow()
    {
        QWindow window;
        QVERIFY(!window.isVisible());
        showInFront(&window);
        QVERIFY(window.isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(&window));
    }

    void bringsBackAMinimizedWindow()
    {
        QWindow window;
        window.show();
        window.setWindowStates(Qt::WindowMinimized);
        showInFront(&window);
        QVERIFY(window.isVisible());
        QVERIFY(!(window.windowStates() & Qt::WindowMinimized));
    }

    void leavesItMaximized()
    {
        QWindow window;
        window.showMaximized();
        showInFront(&window);
        QVERIFY(window.windowStates() & Qt::WindowMaximized);
    }

    void givesItTheFocus()
    {
        QWindow window;
        QWindow other;
        window.show();
        other.show();
        QVERIFY(QTest::qWaitForWindowExposed(&other));
        QTRY_COMPARE(QGuiApplication::focusWindow(), &other);
        showInFront(&window);
        QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    }
};

QTEST_MAIN(TestShowWindow)
#include "tst_showwindow.moc"
