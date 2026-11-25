// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "app/fonts.h"
#include "core/parser.h"
#include "core/sessionguide.h"
#include "core/slots.h"
#include "core/weights.h"
#include "fakegameinput.h"
#include "fakegamewatcher.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "fakepob.h"
#include "fakesecretstore.h"
#include "itemtexts.h"
#include "mainwindowtest.h"
#include "services/datafiles.h"
#include "stylefortests.h"
#include "tradefixtures.h"
#include "updatetexts.h"

#include <KLocalizedQmlContext>
#include <QAccessible>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQuickTest/quicktest.h>
#include <functional>
#include <memory>
#include <optional>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using namespace pou::windowtest;

class TestMainWindowLooks : public QObject
{
    Q_OBJECT

public:
    // The window isn't for the screen. Kirigami would ask the desktop portal
    // for the look of the desktop, which starts it.
    static void initMain()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // the app's own controls, as the program has
        useStyleInTests();
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    }

private Q_SLOTS:
    void initTestCase()
    {
        // so the desktop's color settings are neither read nor written
        QStandardPaths::setTestModeEnabled(true);
        // a container can have no fonts at all, and the font pickers need one
        QVERIFY(addBundledFonts());
    }

    void init()
    {
        // Debian 13's Qt 6.8 now and then warns from its own animation timer that the
        // driver isn't running (Arch and Windows never do), so only that one passes there
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        QTest::failOnWarning(
            QRegularExpression(u"^(?!QUnifiedTimer::stopAnimationDriver: driver is not running$)"_s));
#else
        QTest::failOnWarning();
#endif
    }

    void appearanceDefaults()
    {
        Window w;
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(w.find(u"colorsBox"_s)->property("currentText").toString(), u"Path of Upgrading"_s);
        QCOMPARE(w.find(u"decorationsBox"_s)->property("currentText").toString(), u"Full"_s);
        // there's no scheme in this window, and the page says so
        QCOMPARE(w.find(u"colorsError"_s)->property("visible").toBool(), true);
        QCOMPARE(text(w.find(u"colorsError"_s)), w.app->appearance()->colorsError());
    }

    void appearanceSettings()
    {
        Window w(true, false, QStringLiteral(COLORS));
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(w.find(u"colorsError"_s)->property("visible").toBool(), false);
        QVERIFY(activate(w.find(u"colorsBox"_s), 1));
        QCOMPARE(w.settings().appearance.colors, ColorTheme::Desktop);
        QVERIFY(activate(w.find(u"decorationsBox"_s), 1));
        QCOMPARE(w.settings().appearance.decorations, Decorations::Plain);
        QVERIFY(activate(w.find(u"colorsBox"_s), 0));
        QCOMPARE(w.settings().appearance.colors, ColorTheme::App);
    }

    void theDesignIsPickedOnTheAppearancePage()
    {
        Window w(true, false, QStringLiteral(COLORS));
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(w.find(u"designBox"_s)->property("currentText").toString(), u"Dossier"_s);
        // Dossier has one dark scheme, so there's no brightness to pick
        QCOMPARE(w.find(u"brightnessBox"_s)->property("visible").toBool(), false);

        QVERIFY(activate(w.find(u"designBox"_s), 1));
        QCOMPARE(w.settings().appearance.design, Design::Material);
        QCOMPARE(w.find(u"brightnessBox"_s)->property("visible").toBool(), true);
        QVERIFY(activate(w.find(u"brightnessBox"_s), 2));
        QCOMPARE(w.settings().appearance.brightness, Brightness::Dark);

        // the desktop's own colors are neither light nor dark by choice
        QVERIFY(activate(w.find(u"colorsBox"_s), 1));
        QCOMPARE(w.find(u"brightnessBox"_s)->property("visible").toBool(), false);

        QVERIFY(activate(w.find(u"designBox"_s), 0));
        QCOMPARE(w.settings().appearance.design, Design::Dossier);
    }

    // Every look once, as light and dark only swap the color scheme, but
    // Material's own colors follow the brightness too. Each row loads every
    // page, so this keeps the test within its time.
    void everyPageLoadsUnderEachDesign_data()
    {
        QTest::addColumn<QString>("design");
        QTest::addColumn<QString>("brightness");
        for (const DesignInfo &info : designs())
            QTest::newRow(qPrintable(info.id)) << info.id << (info.hasLight ? u"dark"_s : u"desktop"_s);
        QTest::newRow("material light") << u"material"_s << u"light"_s;
    }

    // QTest::failOnWarning() would catch a page that can't load or bind
    void everyPageLoadsUnderEachDesign()
    {
        QFETCH(QString, design);
        QFETCH(QString, brightness);
        Window w(true, false, QStringLiteral(COLORS));
        w.app->appearance()->setBrightness(brightness);
        w.app->appearance()->setDesign(design);
        QVERIFY(w.shown());
        for (const QString &action : {u"gearAction"_s, u"charactersAction"_s, u"gearScoreAction"_s,
                                      u"overlayAction"_s, u"appearanceAction"_s}) {
            QVERIFY2(w.show(action), qPrintable(action));
            QVERIFY(w.settled());
        }
    }

    void theLookIsPickedInTheSetupGuideToo()
    {
        Window w(true, false, QStringLiteral(COLORS), std::nullopt, false);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        // welcome, league, character, then the look
        for (int i = 0; i < 3; ++i)
            QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(guide->property("title").toString(), u"Pick a Look"_s);
        QVERIFY(isVisible(w.inGuide(u"lookStep"_s)));
        // a swatch for each, with the one it has picked
        for (const DesignInfo &info : designs())
            QVERIFY2(w.inGuide(u"lookSwatch:"_s + info.id), qPrintable(info.id));
        QCOMPARE(w.settings().appearance.design, Design::Dossier);
        QVERIFY(w.inGuide(u"lookSwatch:dossier"_s)->property("current").toBool());

        QVERIFY(click(w.inGuide(u"lookSwatch:material"_s)));
        QCOMPARE(w.settings().appearance.design, Design::Material);
        QVERIFY(w.inGuide(u"lookSwatch:material"_s)->property("current").toBool());
        QVERIFY(!w.inGuide(u"lookSwatch:dossier"_s)->property("current").toBool());
        // which is the choice Appearance has
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(w.find(u"designBox"_s)->property("currentText").toString(), u"Material"_s);
    }

    void styleFollowsTheDesign()
    {
        Window w(true, false, QStringLiteral(COLORS));
        auto *style = w.qml.singletonInstance<QObject *>("PathOfUpgrading", "Style");
        QVERIFY(style);
        QCOMPARE(style->property("material").toBool(), false);
        QCOMPARE(style->property("dark").toBool(), true);

        w.app->appearance()->setBrightness(u"light"_s);
        w.app->appearance()->setDesign(u"material"_s);
        QCOMPARE(style->property("material").toBool(), true);
        QCOMPARE(style->property("dark").toBool(), false);

        w.app->appearance()->setBrightness(u"dark"_s);
        QCOMPARE(style->property("dark").toBool(), true);

        w.app->appearance()->setDesign(u"dossier"_s);
        QCOMPARE(style->property("material").toBool(), false);
        QCOMPARE(style->property("dark").toBool(), true);
    }

    // the popup and the results window read it from the same engine
    void styleFollowsTheColors()
    {
        Window w(true, false, QStringLiteral(COLORS));
        auto *style = w.qml.singletonInstance<QObject *>("PathOfUpgrading", "Style");
        QVERIFY(style);
        QCOMPARE(style->property("appColors").toBool(), true);
        w.app->appearance()->setColors(u"desktop"_s);
        QCOMPARE(style->property("appColors").toBool(), false);
        w.app->appearance()->setColors(u"app"_s);
        QCOMPARE(style->property("appColors").toBool(), true);
    }

    void styleFollowsTheDecorations()
    {
        Window w(true, false, QStringLiteral(COLORS));
        auto *style = w.qml.singletonInstance<QObject *>("PathOfUpgrading", "Style");
        QVERIFY(style);
        QCOMPARE(style->property("plain").toBool(), false);
        w.app->appearance()->setDecorations(u"plain"_s);
        QCOMPARE(style->property("plain").toBool(), true);
        w.app->appearance()->setDecorations(u"full"_s);
        QCOMPARE(style->property("plain").toBool(), false);
    }

    void showsAppearanceSettings()
    {
        Window w;
        Settings settings = w.settings();
        settings.appearance.colors = ColorTheme::Desktop;
        settings.appearance.decorations = Decorations::Plain;
        QVERIFY(w.app->settings().set(settings));
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(w.find(u"colorsBox"_s)->property("currentText").toString(), u"Your desktop's"_s);
        QCOMPARE(w.find(u"decorationsBox"_s)->property("currentText").toString(), u"Plain"_s);
        // and nothing to say about colors it wasn't asked for
        QCOMPARE(w.find(u"colorsError"_s)->property("visible").toBool(), false);
    }

    void headingStyleFollowsTheFonts()
    {
        Window w(true, false, QStringLiteral(COLORS));
        auto *style = w.qml.singletonInstance<QObject *>("PathOfUpgrading", "Style");
        QVERIFY(style);
        QCOMPARE(style->property("headingFamily").toString(), u"Big Shoulders Display"_s);
        const QString family = scalableFamilies().value(0);
        w.app->appearance()->setHeadingFont(family);
        QCOMPARE(style->property("headingFamily").toString(), family);
        w.app->appearance()->setHeadingFont(QString());
        QCOMPARE(style->property("headingFamily").toString(), u"Big Shoulders Display"_s);
    }

    void fontsDefaults()
    {
        Window w;
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(w.find(u"headingFontButton"_s)->property("description").toString(),
                 u"Big Shoulders Display"_s);
        QCOMPARE(w.find(u"textFontButton"_s)->property("description").toString(), u"Spectral"_s);
        QCOMPARE(w.find(u"desktopSizeSwitch"_s)->property("checked").toBool(), true);
        QVERIFY(!isVisible(w.find(u"textSize"_s)));
        QVERIFY(!isVisible(w.find(u"fontsError"_s)));
        QVERIFY(!isVisible(w.find(u"headingFontPicker"_s)));
    }

    void showsFontSettings()
    {
        Window w;
        Settings settings = w.settings();
        settings.appearance.headingFont = u"text"_s;
        settings.appearance.textFont = scalableFamilies().value(0);
        settings.appearance.textSize = 12.5;
        QVERIFY(w.app->settings().set(settings));
        QVERIFY(w.show(u"appearanceAction"_s));
        QCOMPARE(w.find(u"headingFontButton"_s)->property("description").toString(),
                 u"Same as the text font"_s);
        QCOMPARE(w.find(u"textFontButton"_s)->property("description").toString(),
                 settings.appearance.textFont);
        QCOMPARE(w.find(u"desktopSizeSwitch"_s)->property("checked").toBool(), false);
        QVERIFY(isVisible(w.find(u"textSize"_s)));
        QCOMPARE(w.spinBox(u"textSize"_s)->property("value").toInt(), 125);
    }

    void pickHeadingFont()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"appearanceAction"_s));
        QObject *dialog = w.find(u"headingFontPicker"_s);
        QVERIFY(click(w.find(u"headingFontButton"_s)));
        QVERIFY(isVisible(dialog));
        // the default heading font and the same as the text come first
        QVERIFY(w.in(u"headingFontPicker"_s, u"font:"_s));
        QVERIFY(w.settled());
        QVERIFY(click(w.in(u"headingFontPicker"_s, u"font:text"_s)));
        QCOMPARE(w.settings().appearance.headingFont, u"text"_s);
        QTRY_VERIFY(!isVisible(dialog));
        QCOMPARE(w.find(u"headingFontButton"_s)->property("description").toString(),
                 u"Same as the text font"_s);

        const QString family = scalableFamilies().value(0);
        QVERIFY(click(w.find(u"headingFontButton"_s)));
        QVERIFY(w.settled());
        QVERIFY(click(w.in(u"headingFontPicker"_s, u"font:"_s + family)));
        QCOMPARE(w.settings().appearance.headingFont, family);
        QCOMPARE(w.find(u"headingFontButton"_s)->property("description").toString(), family);
    }

    void pickTextFont()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"appearanceAction"_s));
        const QString family = scalableFamilies().value(0);
        QVERIFY(click(w.find(u"textFontButton"_s)));
        QVERIFY(w.in(u"textFontPicker"_s, u"fontSearch"_s)->setProperty("text", family.toLower()));
        QVERIFY(w.settled());
        QVERIFY(press(w.in(u"textFontPicker"_s, u"fontSearch"_s), Qt::Key_Return));
        QCOMPARE(w.settings().appearance.textFont.toLower(), family.toLower());
        QCOMPARE(QGuiApplication::font().family().toLower(), family.toLower());

        // and back to the default, which is first when there are no words
        QVERIFY(click(w.find(u"textFontButton"_s)));
        QCOMPARE(text(w.in(u"textFontPicker"_s, u"fontSearch"_s)), QString());
        QVERIFY(press(w.in(u"textFontPicker"_s, u"fontSearch"_s), Qt::Key_Return));
        QCOMPARE(w.settings().appearance.textFont, QString());
    }

    void pickFontByKeys()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"appearanceAction"_s));
        QVERIFY(click(w.find(u"textFontButton"_s)));
        QObject *search = w.in(u"textFontPicker"_s, u"fontSearch"_s);
        QVERIFY(w.settled());
        // the default is first, so the next one is the first font
        QVERIFY(press(search, Qt::Key_Down));
        QVERIFY(press(search, Qt::Key_Return));
        QCOMPARE(w.settings().appearance.textFont, scalableFamilies().value(0));
    }

    void noFontFound()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"appearanceAction"_s));
        QVERIFY(click(w.find(u"textFontButton"_s)));
        QVERIFY(!isVisible(w.in(u"textFontPicker"_s, u"noFonts"_s)));
        QVERIFY(w.in(u"textFontPicker"_s, u"fontSearch"_s)->setProperty("text", u"no such font anywhere"_s));
        QVERIFY(w.settled());
        QVERIFY(isVisible(w.in(u"textFontPicker"_s, u"noFonts"_s)));
        // nothing to pick, so enter changes nothing
        QVERIFY(press(w.in(u"textFontPicker"_s, u"fontSearch"_s), Qt::Key_Return));
        QCOMPARE(w.settings().appearance.textFont, QString());
    }

    void chooseTextSize()
    {
        Window w;
        QVERIFY(w.show(u"appearanceAction"_s));
        const double desktop = w.app->appearance()->desktopTextSize();
        QVERIFY(toggle(w.find(u"desktopSizeSwitch"_s)));
        QCOMPARE(w.settings().appearance.textSize, std::round(desktop));
        QVERIFY(isVisible(w.find(u"textSize"_s)));

        QVERIFY(setValue(w.spinBox(u"textSize"_s), 135, "valueModified"));
        QCOMPARE(w.settings().appearance.textSize, 13.5);
        QCOMPARE(QGuiApplication::font().pointSizeF(), 13.5);

        QVERIFY(toggle(w.find(u"desktopSizeSwitch"_s)));
        QCOMPARE(w.settings().appearance.textSize, 0.0);
        QCOMPARE(QGuiApplication::font().pointSizeF(), desktop);
        QVERIFY(!isVisible(w.find(u"textSize"_s)));
    }

    void saysWhichFontIsMissing()
    {
        Window w;
        Settings settings = w.settings();
        settings.appearance.textFont = u"Made Up Grotesque"_s;
        QVERIFY(w.app->settings().set(settings));
        QVERIFY(w.show(u"appearanceAction"_s));
        QObject *error = w.find(u"fontsError"_s);
        QVERIFY(isVisible(error));
        QCOMPARE(text(error), u"Made Up Grotesque isn't installed, so the default is used."_s);
    }
};

QTEST_MAIN(TestMainWindowLooks)
#include "tst_mainwindow_looks.moc"
