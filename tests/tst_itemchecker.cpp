// SPDX-License-Identifier: MIT
#include "app/itemchecker.h"
#include "core/parser.h"
#include "core/placement.h"
#include "fakegameinput.h"
#include "fakepob.h"
#include "itemtexts.h"
#include "services/pobengine.h"

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

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/></PathOfBuilding2>"_s;

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

// Path of Building's answer for the helmet, with the build's numbers as they
// are and with it on.
QJsonObject withHelmet(const QJsonObject &stats)
{
    const QJsonObject before{{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1000}};
    const QJsonObject place{{u"slot"_s, u"Helmet"_s},
                            {u"label"_s, u"Helmet"_s},
                            {u"replacing"_s, u"Grim Crown"_s},
                            {u"stats"_s, stats}};
    return {{u"before"_s, before},
            {u"results"_s, QJsonArray{QJsonObject{{u"ok"_s, true}, {u"slots"_s, QJsonArray{place}}}}}};
}

// The checker with Path of Building, which has the build.
struct WithPob
{
    QQmlEngine qml;
    FakeGameInput input;
    FakePob pob;
    PobEngine engine;
    ItemChecker checker{&qml, &input};

    WithPob()
    {
        engine.setInstall(pob.install());
        engine.setBuild(build);
        checker.setPob(&engine);
        checker.setCharacter(inGrimCrown(), PobBuild());
    }

    // the report on the helmet once Path of Building has answered
    ItemReport answered(const QJsonObject &stats)
    {
        pob.answers(withHelmet(stats));
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        if (!QTest::qWaitFor([&] { return !checker.popup().report().pobChecking; }))
            return {};
        return checker.popup().report();
    }
};

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
        checker.setCharacter(inGrimCrown(), std::nullopt);
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

    void pobAnswers()
    {
        WithPob p;
        p.pob.answers(withHelmet({{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}}));
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(p.checker.popup().report().pobChecking);

        QTRY_VERIFY(!p.checker.popup().report().pobChecking);
        QCOMPARE(p.checker.popup().report().name, u"Doom Visage"_s);
        QVERIFY(!p.checker.popup().report().headline.isEmpty());
        // still by the mouse where you checked it
        QTRY_COMPARE(p.checker.popup().window()->position(),
                     placed(p.checker.popup(), PopupPosition::Cursor, QPoint(200, 300)));
    }

    void staysClosed()
    {
        // when you close it before Path of Building answers
        WithPob p;
        p.pob.answers(withHelmet({{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}}));
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        p.checker.popup().close();
        QTRY_VERIFY(!p.checker.popup().report().pobChecking);
        QVERIFY(!p.checker.popup().isVisible());
    }

    void staysOpen()
    {
        // with the rest showing, when you opened it up first
        WithPob p;
        p.pob.answers(withHelmet({{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}}));
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        p.checker.popup().setExpanded(true);
        QTRY_VERIFY(!p.checker.popup().report().pobChecking);
        QVERIFY(p.checker.popup().isExpanded());
    }

    void pobOff()
    {
        WithPob p;
        Settings settings;
        settings.pob.enabled = false;
        p.checker.setSettings(settings);
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(!p.checker.popup().report().pobChecking);

        // and once it's on again
        p.checker.setSettings({});
        p.checker.check();
        Q_EMIT p.input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(p.checker.popup().report().pobChecking);
    }

    void pobOffFirst()
    {
        // before there's a Path of Building to use
        QQmlEngine qml;
        FakeGameInput input;
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        ItemChecker checker(&qml, &input);
        checker.setCharacter(inGrimCrown(), PobBuild());
        Settings settings;
        settings.pob.enabled = false;
        checker.setSettings(settings);
        checker.setPob(&engine);
        checker.check();
        Q_EMIT input.itemCopied(itemtexts::helmetNew, mouseAt(200, 300));
        QVERIFY(!checker.popup().report().pobChecking);
    }

    void yourDamageWeight()
    {
        // 10% more damage and no more life
        const QJsonObject harder{{u"FullDPS"_s, 110}, {u"TotalEHP"_s, 1000}};
        WithPob p;
        QCOMPARE(p.answered(harder).verdict, u"Upgrade"_s);

        Settings settings;
        settings.pob.damageWeight = 0;
        p.checker.setSettings(settings);
        QCOMPARE_NE(p.answered(harder).verdict, u"Upgrade"_s);
    }

    void yourPriorities()
    {
        // a fifth of your damage for half again as much life
        const QJsonObject tougher{{u"FullDPS"_s, 80}, {u"TotalEHP"_s, 1500}};
        WithPob p;
        QVERIFY(!p.answered(tougher).losses.isEmpty());

        Settings settings;
        settings.pob.priorities.keepDamage = false;
        p.checker.setSettings(settings);
        QVERIFY(p.answered(tougher).losses.isEmpty());
    }
};

QTEST_MAIN(TestItemChecker)
#include "tst_itemchecker.moc"
