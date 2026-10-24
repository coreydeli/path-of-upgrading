// SPDX-License-Identifier: MIT
#include "app/itempopup.h"
#include "core/placement.h"
#include "fakegameinput.h"

#include <QColor>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QtMath>
#include <algorithm>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

ItemReport aReport()
{
    ItemReport report;
    report.name = u"Storm Veil"_s;
    report.rarity = u"Rare"_s;
    report.details = {u"Hunter Hood"_s, u"Helmet"_s, u"Item level 74"_s};
    report.verdict = u"Upgrade"_s;
    report.verdictTone = Tone::Good;
    return report;
}

ScreenPoint mouseAt(int x, int y)
{
    return {.screen = QGuiApplication::primaryScreen()->name(), .position = {x, y}};
}

// Where it should be for the mouse, on the screen.
QPoint placed(const QWindow *window, PopupPosition where, const std::optional<QPoint> &mouse)
{
    QScreen *screen = QGuiApplication::primaryScreen();
    return screen->geometry().topLeft() + placePopup(where, window->size(), screen->size(), mouse);
}

void addVisible(QQuickItem *parent, QList<QQuickItem *> &items)
{
    const QList<QQuickItem *> children = parent->childItems();
    for (QQuickItem *item : children) {
        if (item->isVisible()) {
            items << item;
            addVisible(item, items);
        }
    }
}

// What you can see on the popup.
QList<QQuickItem *> visibleItems(const ItemPopup &popup)
{
    QList<QQuickItem *> items;
    addVisible(popup.window()->contentItem(), items);
    return items;
}

// What on the popup you can see with that text, a label or a button.
QQuickItem *shown(const ItemPopup &popup, const QString &text, const char *type = "QQuickItem")
{
    const QList<QQuickItem *> items = visibleItems(popup);
    const auto it = std::ranges::find_if(items, [&](const QQuickItem *item) {
        return item->inherits(type) && item->property("text").toString() == text;
    });
    return it != items.end() ? *it : nullptr;
}

// Whether it shows it's waiting for something.
bool busy(const ItemPopup &popup)
{
    return std::ranges::any_of(visibleItems(popup), [](const QQuickItem *item) {
        return item->inherits("QQuickBusyIndicator") && item->property("running").toBool();
    });
}

QQuickItem *button(const ItemPopup &popup, const QString &text)
{
    return shown(popup, text, "QQuickAbstractButton");
}

// Somewhere it isn't near the popup.
ScreenPoint elsewhere()
{
    return {.screen = u"HDMI-A-9"_s, .position = {0, 0}};
}

} // namespace

class TestItemPopup : public QObject
{
    Q_OBJECT

public:
    // the popup's windows aren't for the screen
    static void initMain() { qputenv("QT_QPA_PLATFORM", "offscreen"); }

private Q_SLOTS:
    void init()
    {
        // which a mistake in the QML only gives
        QTest::failOnWarning();
    }

    void byTheMouse()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        QVERIFY(!popup.isVisible());
        popup.showReport(aReport(), mouseAt(200, 300));
        QVERIFY(popup.isVisible());
        QCOMPARE(popup.report(), aReport());
        QVERIFY(popup.message().isEmpty());
        QCOMPARE(popup.window()->position(), placed(popup.window(), PopupPosition::Cursor, QPoint(200, 300)));
        // over the game, which keeps the keyboard
        QVERIFY(popup.window()->flags().testFlag(Qt::WindowDoesNotAcceptFocus));

        QVERIFY(shown(popup, u"Storm Veil"_s));
        QVERIFY(shown(popup, u"Hunter Hood, Helmet, Item level 74"_s));
        QVERIFY(shown(popup, u"Upgrade"_s));
    }

    void mouseUnknown()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), std::nullopt);
        QVERIFY(popup.isVisible());
        QCOMPARE(popup.window()->position(), placed(popup.window(), PopupPosition::Top, std::nullopt));
    }

    void whereTheSettingsSay()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        OverlaySettings settings;
        settings.position = PopupPosition::Right;
        popup.setSettings(settings);
        popup.showReport(aReport(), mouseAt(200, 300));
        QCOMPARE(popup.window()->position(), placed(popup.window(), PopupPosition::Right, QPoint(200, 300)));
    }

    void aMessage()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        popup.showMessage(u"That isn't an item."_s, mouseAt(200, 300));
        QVERIFY(popup.isVisible());
        QCOMPARE(popup.message(), u"That isn't an item."_s);
        QCOMPARE(popup.report(), ItemReport());
        QVERIFY(shown(popup, u"That isn't an item."_s));
        QVERIFY(!shown(popup, u"Storm Veil"_s));
        QVERIFY(!shown(popup, u"Upgrade"_s));

        popup.showReport(aReport(), mouseAt(200, 300));
        QVERIFY(popup.message().isEmpty());
        QCOMPARE(popup.report(), aReport());
    }

    void sections_data()
    {
        QTest::addColumn<ItemReport>("report");
        QTest::addColumn<QStringList>("texts");

        ItemReport scored = aReport();
        scored.score = u"+4%"_s;
        scored.scoreLabel = u"for your build"_s;
        scored.versus = u"vs Grim Crown"_s;
        QTest::newRow("score") << scored << QStringList{u"+4%"_s, u"for your build, vs Grim Crown"_s};

        ItemReport equipped = aReport();
        equipped.verdict = u"Equipped"_s;
        equipped.versus = u"in your Helmet slot"_s;
        QTest::newRow("equipped") << equipped << QStringList{u"Equipped"_s, u"in your Helmet slot"_s};

        ItemReport costly = aReport();
        costly.losses = {u"Fire res drops below the cap"_s, u"Cold res drops below the cap"_s};
        costly.risks = {u"Minion skills lose a level"_s};
        costly.caps = u"Caps your lightning resistance"_s;
        QTest::newRow("losses, risks and caps")
            << costly
            << QStringList{u"Fire res drops below the cap\nCold res drops below the cap"_s,
                           u"Minion skills lose a level"_s, u"Caps your lightning resistance"_s};

        ItemReport judged = aReport();
        judged.headline = {
            ReportFigure{.label = u"DPS"_s, .change = u"+6.1%"_s, .tone = Tone::Good},
            ReportFigure{.label = u"Effective HP"_s, .change = u"-2.3%"_s, .tone = Tone::Bad},
        };
        QTest::newRow("Path of Building's figures")
            << judged << QStringList{u"+6.1%"_s, u"DPS"_s, u"-2.3%"_s, u"Effective HP"_s};

        ItemReport stats = aReport();
        stats.stats = {
            ReportFigure{.label = u"+45 to maximum Life"_s, .tone = Tone::Good},
            ReportFigure{.label = u"-30 to Spirit"_s, .tone = Tone::Bad},
        };
        QTest::newRow("stats") << stats << QStringList{u"+45 to maximum Life"_s, u"-30 to Spirit"_s};

        ItemReport changes = aReport();
        changes.changes = {
            ReportFigure{.label = u"Fire res"_s,
                         .change = u"-8%"_s,
                         .from = u"75%"_s,
                         .to = u"67%"_s,
                         .note = u"off the cap"_s,
                         .tone = Tone::Bad},
            // when only the change is known
            ReportFigure{.label = u"Cold res"_s, .change = u"+12%"_s, .tone = Tone::Good},
        };
        ItemReport rated = aReport();
        rated.gearScore = u"78"_s;
        rated.gearGrade = u"A"_s;
        rated.gearBuild = u"Minions"_s;
        rated.yourGearScore = u"64"_s;
        QTest::newRow("gear score") << rated << QStringList{u"78 A"_s, u"gear score for Minions, yours 64"_s};

        ItemReport firstRated = aReport();
        firstRated.gearScore = u"78"_s;
        firstRated.gearGrade = u"A"_s;
        QTest::newRow("gear score in an empty slot") << firstRated << QStringList{u"78 A"_s, u"gear score"_s};

        ItemReport craftable = aReport();
        craftable.openAffixes = u"Open affixes: 1 prefix, 2 suffixes"_s;
        QTest::newRow("open affixes") << craftable << QStringList{u"Open affixes: 1 prefix, 2 suffixes"_s};

        ItemReport noBuild = aReport();
        noBuild.pobStatus = u"Import this character's build from Path of Building for exact verdicts."_s;
        QTest::newRow("Path of Building's status")
            << noBuild
            << QStringList{u"Import this character's build from Path of Building for exact verdicts."_s};

        ItemReport noted = aReport();
        noted.notes = {u"Two-handed, so it takes your off hand too."_s, u"It's unidentified."_s};
        QTest::newRow("notes") << noted << noted.notes;

        QTest::newRow("changes") << changes
                                 << QStringList{u"Now"_s, u"With it"_s,     u"Fire res"_s, u"75%"_s, u"67%"_s,
                                                u"-8%"_s, u"off the cap"_s, u"Cold res"_s, u"+12%"_s};
    }

    void sections()
    {
        QFETCH(ItemReport, report);
        QFETCH(QStringList, texts);

        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(report, mouseAt(200, 300));
        for (const QString &text : std::as_const(texts)) {
            if (!shown(popup, text))
                QFAIL(qPrintable(u"It doesn't show \"%1\""_s.arg(text)));
        }
    }

    void checking()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        ItemReport report = aReport();
        report.pobChecking = true;
        report.pobStatus = u"Checking with Path of Building"_s;
        popup.showReport(report, mouseAt(200, 300));
        QVERIFY(shown(popup, u"Checking with Path of Building"_s));
        QVERIFY(busy(popup));

        // and when it's answered
        popup.updateReport(aReport());
        QVERIFY(!shown(popup, u"Checking with Path of Building"_s));
        QVERIFY(!busy(popup));

        // or couldn't
        report.pobChecking = false;
        report.pobStatus = u"There's nowhere in the build this item can go."_s;
        popup.updateReport(report);
        QVERIFY(shown(popup, report.pobStatus));
        QVERIFY(!busy(popup));
    }

    void updated()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        const QPoint at = popup.window()->position();

        ItemReport newer = aReport();
        newer.verdict = u"Sidegrade"_s;
        popup.updateReport(newer);
        QVERIFY(popup.isVisible());
        QCOMPARE(popup.report(), newer);
        QCOMPARE(popup.window()->position(), at);
    }

    void updatedWhenHidden()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        popup.close();
        popup.updateReport(aReport());
        QVERIFY(!popup.isVisible());
    }

    void expanded()
    {
        // until the next item, which Path of Building's answer isn't
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        QSignalSpy changed(&popup, &ItemPopup::expandedChanged);
        popup.showReport(aReport(), mouseAt(200, 300));
        QVERIFY(!popup.isExpanded());
        QCOMPARE(changed.size(), 0);
        popup.setExpanded(true);
        popup.setExpanded(true);
        QCOMPARE(changed.size(), 1);

        popup.updateReport(aReport());
        QVERIFY(popup.isExpanded());
        popup.showReport(aReport(), mouseAt(200, 300));
        QVERIFY(!popup.isExpanded());
        QCOMPARE(changed.size(), 2);

        popup.setExpanded(true);
        popup.showMessage(u"That isn't an item."_s, mouseAt(200, 300));
        QVERIFY(!popup.isExpanded());
    }

    void grows()
    {
        // near the bottom of the screen, where it has to go up as it grows
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        const int bottom = QGuiApplication::primaryScreen()->size().height() - 10;
        popup.showReport(aReport(), mouseAt(200, bottom));
        const int height = popup.window()->height();

        // as it does when Path of Building answers, here with a name long
        // enough to wrap
        ItemReport longer = aReport();
        longer.name = u"Storm Veil of the Long Winter Kept Behind the Gate of the Drowned City"_s;
        popup.updateReport(longer);
        QTRY_VERIFY(popup.window()->height() > height);
        QCOMPARE(popup.window()->position(),
                 placed(popup.window(), PopupPosition::Cursor, QPoint(200, bottom)));
    }

    void colours()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        // the game's for a rare
        QCOMPARE(shown(popup, u"Storm Veil"_s)->property("color"), QColor(u"#ffff77"_s));
        const QColor upgrade = shown(popup, u"Upgrade"_s)->property("color").value<QColor>();
        QCOMPARE_NE(upgrade, QColor(u"#3daee9"_s));

        OverlaySettings settings;
        settings.colorblind = true;
        popup.setSettings(settings);
        QCOMPARE(shown(popup, u"Upgrade"_s)->property("color"), QColor(u"#3daee9"_s));
    }

    void tones_data()
    {
        // in the colours for the colourblind, which don't follow the theme
        QTest::addColumn<ItemReport>("report");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QColor>("colour");

        const QColor better(u"#3daee9"_s);
        const QColor worse(u"#f67400"_s);
        const QColor careful(u"#fdbc4b"_s);

        ItemReport scored = aReport();
        scored.verdict = u"Risky upgrade"_s;
        scored.verdictTone = Tone::Warning;
        scored.score = u"+4%"_s;
        QTest::newRow("verdict") << scored << u"Risky upgrade"_s << careful;
        QTest::newRow("score") << scored << u"+4%"_s << careful;

        ItemReport capping = aReport();
        capping.caps = u"Caps your fire resistance"_s;
        QTest::newRow("caps") << capping << u"Caps your fire resistance"_s << better;

        ItemReport judged = aReport();
        judged.headline = {ReportFigure{.label = u"DPS"_s, .change = u"-6.1%"_s, .tone = Tone::Bad}};
        QTest::newRow("Path of Building's figures") << judged << u"-6.1%"_s << worse;

        ItemReport stats = aReport();
        stats.stats = {ReportFigure{.label = u"-30 to Spirit"_s, .tone = Tone::Bad}};
        QTest::newRow("stats") << stats << u"-30 to Spirit"_s << worse;

        ItemReport changes = aReport();
        changes.changes = {ReportFigure{.label = u"Cold res"_s,
                                        .change = u"+12%"_s,
                                        .from = u"63%"_s,
                                        .to = u"75%"_s,
                                        .note = u"capped"_s,
                                        .tone = Tone::Good}};
        QTest::newRow("a change") << changes << u"+12%"_s << better;
        QTest::newRow("its note") << changes << u"capped"_s << better;
    }

    void tones()
    {
        QFETCH(ItemReport, report);
        QFETCH(QString, text);
        QFETCH(QColor, colour);

        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        OverlaySettings settings;
        settings.colorblind = true;
        popup.setSettings(settings);
        popup.showReport(report, mouseAt(200, 300));
        QQuickItem *label = shown(popup, text);
        QVERIFY(label);
        QCOMPARE(label->property("color"), colour);
    }

    void scaled()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        const QQuickItem *card = popup.window()->contentItem()->childItems().value(0);
        QVERIFY(card);
        QCOMPARE(popup.window()->size(), QSize(qCeil(card->implicitWidth()), qCeil(card->implicitHeight())));

        OverlaySettings settings;
        settings.scale = 1.5;
        popup.setSettings(settings);
        QCOMPARE(popup.scale(), 1.5);
        const QSize scaled(qCeil(card->implicitWidth() * 1.5), qCeil(card->implicitHeight() * 1.5));
        QTRY_COMPARE(popup.window()->size(), scaled);
    }

    void closed()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        QSignalSpy hovered(&popup, &ItemPopup::hoveredChanged);
        popup.showReport(aReport(), mouseAt(200, 300));
        popup.setHovered(true);
        popup.setHovered(true);
        popup.close();
        QVERIFY(!popup.isVisible());
        QVERIFY(!popup.isHovered());
        QCOMPARE(hovered.size(), 2);

        popup.showReport(aReport(), mouseAt(200, 300));
        QVERIFY(popup.isVisible());
    }

    void closeButton()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        QVERIFY(QTest::qWaitForWindowExposed(popup.window()));
        QQuickItem *close = button(popup, u"Close"_s);
        QVERIFY(close);
        const QPoint middle = close->mapToScene(close->boundingRect().center()).toPoint();
        QTest::mouseClick(popup.window(), Qt::LeftButton, {}, middle);
        QVERIFY(!popup.isVisible());
    }

    void hovered()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        QVERIFY(QTest::qWaitForWindowExposed(popup.window()));
        QTest::mouseMove(popup.window(), QPoint(20, 20));
        QTRY_VERIFY(popup.isHovered());
    }

    void hidesByItself()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        OverlaySettings settings;
        settings.autoHideSeconds = 2;
        popup.setSettings(settings);
        popup.showReport(aReport(), mouseAt(200, 300));
        QTest::qWait(1500);
        QVERIFY(popup.isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(!popup.isVisible(), 2000);
    }

    void staysWhileHovered()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        OverlaySettings settings;
        settings.autoHideSeconds = 2;
        popup.setSettings(settings);
        popup.showReport(aReport(), mouseAt(200, 300));
        popup.setHovered(true);
        QTest::qWait(2500);
        QVERIFY(popup.isVisible());
    }

    void hidesWhenYouGoAway()
    {
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        input.mouseAt = elsewhere();
        QTRY_VERIFY_WITH_TIMEOUT(!popup.isVisible(), 2000);
    }

    void stopsLooking()
    {
        // for the mouse, once it's closed
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        popup.showReport(aReport(), mouseAt(200, 300));
        QTRY_VERIFY(input.mouseLooks > 0);
        popup.close();
        const int looks = input.mouseLooks;
        QTest::qWait(300);
        QCOMPARE(input.mouseLooks, looks);
    }

    void notByTheMouse()
    {
        // at the side of the screen, where you go past it to the item
        QQmlEngine engine;
        FakeGameInput input;
        ItemPopup popup(&engine, &input);
        OverlaySettings settings;
        settings.position = PopupPosition::Right;
        popup.setSettings(settings);
        popup.showReport(aReport(), mouseAt(200, 300));
        input.mouseAt = elsewhere();
        QTest::qWait(1500);
        QVERIFY(popup.isVisible());
    }
};

QTEST_MAIN(TestItemPopup)
#include "tst_itempopup.moc"
