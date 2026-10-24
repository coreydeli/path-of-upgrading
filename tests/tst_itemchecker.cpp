// SPDX-License-Identifier: MIT
#include "app/itemchecker.h"
#include "core/parser.h"
#include "core/placement.h"
#include "fakegameinput.h"
#include "itemtexts.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

ScreenPoint mouseAt(int x, int y)
{
    return {.screen = QGuiApplication::primaryScreen()->name(), .position = {x, y}};
}

// Where the popup should be for the mouse, on the screen.
QPoint placed(const ItemPopup &popup, PopupPosition where, const std::optional<QPoint> &mouse)
{
    QScreen *screen = QGuiApplication::primaryScreen();
    return screen->geometry().topLeft() + placePopup(where, popup.window()->size(), screen->size(), mouse);
}

Character inGrimCrown()
{
    Character c;
    c.gear = {{u"Helmet"_s, parseItem(itemtexts::helmetOld).value()}};
    return c;
}

} // namespace

class TestItemChecker : public QObject
{
    Q_OBJECT

public:
    // the popup's windows aren't for the screen
    static void initMain() { qputenv("QT_QPA_PLATFORM", "offscreen"); }

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void showsIt()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        checker.setCharacter(inGrimCrown());
        QSignalSpy read(&checker, &ItemChecker::itemRead);
        checker.check();
        QCOMPARE(input.copies, 1);
        QVERIFY(!checker.popup().isVisible());

        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().report().name, u"Doom Visage"_s);
        QCOMPARE(checker.popup().report().versus, u"vs Grim Crown"_s);
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Cursor, QPoint(200, 300)));
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().at(0).value<Item>().name, u"Doom Visage"_s);
    }

    void failed()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        input.failRightAway = u"The game isn't running."_s;
        checker.check();
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().message(), u"The game isn't running."_s);
    }

    void yourHotkey()
    {
        // which it tells you to press again
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.hotkey = u"F3"_s;
        checker.setSettings(settings);
        checker.check();
        Q_EMIT input.copyFailed(GameInput::Failure::NothingCopied, {}, mouseAt(200, 300));
        QVERIFY(checker.popup().message().contains(u"press F3"_s));
    }

    void advancedCopy()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.advancedCopy = true;
        checker.setSettings(settings);
        checker.check();
        QVERIFY(input.lastAdvanced);
    }

    void paused()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.paused = true;
        checker.setSettings(settings);
        checker.check();
        QCOMPARE(input.copies, 0);
    }

    void copiedYourself()
    {
        // where the mouse isn't known
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Q_EMIT input.textCopied(itemtexts::helmetNew);
        QVERIFY(checker.popup().isVisible());
        QCOMPARE(checker.popup().report().name, u"Doom Visage"_s);
        QCOMPARE(checker.popup().window()->position(),
                 placed(checker.popup(), PopupPosition::Top, std::nullopt));
    }

    void notWatching()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.watchClipboard = false;
        checker.setSettings(settings);
        Q_EMIT input.textCopied(itemtexts::helmetNew);
        QVERIFY(!checker.popup().isVisible());
    }

    void popupSettings()
    {
        QQmlEngine qml;
        FakeGameInput input;
        ItemChecker checker(&qml, &input);
        Settings settings;
        settings.overlay.scale = 1.5;
        checker.setSettings(settings);
        QCOMPARE(checker.popup().scale(), 1.5);
    }
};

QTEST_MAIN(TestItemChecker)
#include "tst_itemchecker.moc"
