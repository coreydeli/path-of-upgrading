// SPDX-License-Identifier: MIT
#include "app/appearance.h"
#include "app/fonts.h"
#include "app/settingsstore.h"

#include <KColorScheme>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString colorsFile = QStringLiteral(COLORS);

// The settings and an appearance following them. Leaves the desktop's colors
// on, as the next test starts from them.
struct Look
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    Appearance appearance;

    explicit Look(const QString &file = colorsFile)
        : appearance(&settings, file)
    {}
    explicit Look(const Settings &first, const QString &file = colorsFile)
        : appearance((settings.set(first), &settings), file)
    {}
    ~Look() { appearance.setColors(u"desktop"_s); }

    const AppearanceSettings &saved() const { return settings.settings().appearance; }
};

QColor window()
{
    return QGuiApplication::palette().color(QPalette::Active, QPalette::Window);
}

const QColor appWindow = QColor(u"#100e0c"_s);

// An installed family that isn't the app's font now, or nothing.
QString otherFamily()
{
    const QString now = QGuiApplication::font().family();
    for (const QString &family : scalableFamilies()) {
        if (family != now)
            return family;
    }
    return {};
}

const QString notInstalled = u"Made Up Grotesque"_s;

} // namespace

class TestAppearance : public QObject
{
    Q_OBJECT

public:
    // Kirigami would ask the desktop portal for the look of the desktop,
    // which starts it. Fusion, as on Linux, as Windows' own style can't draw
    // offscreen.
    static void initMain()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
        // Kirigami 6.13, as in Debian 13, warns that the Fusion style has no
        // platform plugin of its own
        qputenv("QT_LOGGING_RULES", "kf.kirigami.platform.warning=false");
    }

private Q_SLOTS:
    void initTestCase()
    {
        // so nothing of the desktop's colors is read or written
        QStandardPaths::setTestModeEnabled(true);
    }

    void init() { QTest::failOnWarning(); }

    void startsWithItsOwnColors()
    {
        Look look;
        QCOMPARE(look.appearance.colors(), u"app"_s);
        QCOMPARE(look.appearance.decorations(), u"full"_s);
        QVERIFY(look.appearance.hasAppColors());
        QVERIFY(!look.appearance.isPlain());
        QCOMPARE(look.appearance.colorsError(), QString());
        QCOMPARE(window(), appWindow);
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Highlight),
                 QColor(u"#b8924a"_s));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::WindowText),
                 QColor(u"#ddd2c2"_s));
    }

    void followsTheSetting()
    {
        Look look;
        QSignalSpy changed(&look.appearance, &Appearance::changed);
        look.appearance.setColors(u"desktop"_s);
        QCOMPARE(look.appearance.colors(), u"desktop"_s);
        QVERIFY(!look.appearance.hasAppColors());
        QVERIFY(window() != appWindow);
        QVERIFY(changed.count() >= 1);

        look.appearance.setColors(u"app"_s);
        QVERIFY(look.appearance.hasAppColors());
        QCOMPARE(window(), appWindow);
    }

    void followsTheSettingsFile()
    {
        // as when something else changes the settings
        Look look;
        Settings settings = look.settings.settings();
        settings.appearance.colors = ColorTheme::Desktop;
        QVERIFY(look.settings.set(settings));
        QVERIFY(!look.appearance.hasAppColors());
        QVERIFY(window() != appWindow);
    }

    void leavesTheDesktopsColorsAlone()
    {
        Settings first;
        first.appearance.colors = ColorTheme::Desktop;
        const QColor before = window();
        Look look(first);
        QVERIFY(!look.appearance.hasAppColors());
        QCOMPARE(look.appearance.colorsError(), QString());
        QCOMPARE(window(), before);
        QVERIFY(window() != appWindow);
    }

    void savesWhatYouChange()
    {
        Look look;
        look.appearance.setColors(u"desktop"_s);
        look.appearance.setDecorations(u"plain"_s);
        QCOMPARE(look.saved().colors, ColorTheme::Desktop);
        QCOMPARE(look.saved().decorations, Decorations::Plain);
        QVERIFY(look.appearance.isPlain());
        QCOMPARE(look.appearance.decorations(), u"plain"_s);

        QSettings again(look.dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        QCOMPARE(again.value(u"appearance/theme"_s).toString(), u"desktop"_s);
        QCOMPARE(again.value(u"appearance/decorations"_s).toString(), u"plain"_s);
    }

    void readsTheSavedChoice()
    {
        Settings first;
        first.appearance.decorations = Decorations::Plain;
        Look look(first);
        QVERIFY(look.appearance.isPlain());
        QCOMPARE(look.appearance.colors(), u"app"_s);
    }

    // the Plasma style reads its colors from the scheme the app names
    void namesTheSchemeForTheStyle()
    {
        Look look;
        QCOMPARE(qApp->property("KDE_COLOR_SCHEME_PATH").toString(), QStringLiteral(COLORS));
        look.appearance.setColors(u"desktop"_s);
        QVERIFY(qApp->property("KDE_COLOR_SCHEME_PATH").toString().isEmpty());
        look.appearance.setColors(u"app"_s);
        QCOMPARE(qApp->property("KDE_COLOR_SCHEME_PATH").toString(), QStringLiteral(COLORS));
    }

    void aMissingSchemeLeavesTheDesktopsColors()
    {
        const QColor before = window();
        Look look(u"/made-up/color-schemes/PathOfUpgrading.colors"_s);
        QCOMPARE(look.appearance.colors(), u"app"_s);
        QVERIFY(!look.appearance.hasAppColors());
        QVERIFY(!look.appearance.colorsError().isEmpty());
        QCOMPARE(window(), before);

        // and the desktop's was chosen anyway, so there's nothing to say
        look.appearance.setColors(u"desktop"_s);
        QCOMPARE(look.appearance.colorsError(), QString());
    }

    void aFileWithoutColorsLeavesTheDesktopsColors()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath(u"Empty.colors"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[General]\nName=Empty\n");
        file.close();
        const QColor before = window();
        Look look(file.fileName());
        QVERIFY(!look.appearance.hasAppColors());
        QVERIFY(!look.appearance.colorsError().isEmpty());
        QCOMPARE(window(), before);
    }

    void unknownIdsAreTheDefaults()
    {
        Look look;
        look.appearance.setColors(u"neon"_s);
        look.appearance.setDecorations(u"lots"_s);
        QCOMPARE(look.appearance.colors(), u"app"_s);
        QCOMPARE(look.appearance.decorations(), u"full"_s);
    }

    void fontsStartAsTheDesktops()
    {
        const QFont desktop = QGuiApplication::font();
        Look look;
        QCOMPARE(look.appearance.headingFont(), QString());
        QCOMPARE(look.appearance.textFont(), QString());
        QCOMPARE(look.appearance.textSize(), 0.0);
        QCOMPARE(look.appearance.headingFamily(), headingFontFamily());
        QCOMPARE(look.appearance.textFamily(), desktop.family());
        QCOMPARE(look.appearance.desktopTextSize(), desktop.pointSizeF());
        QCOMPARE(look.appearance.fontsError(), QString());
        QCOMPARE(QGuiApplication::font(), desktop);
    }

    void listsOnlyFontsThatScale()
    {
        Look look;
        const QStringList families = look.appearance.families();
        QVERIFY(!families.isEmpty());
        for (const QString &family : families) {
            QVERIFY2(QFontDatabase::isSmoothlyScalable(family), qPrintable(family));
            // so setting it takes
            QVERIFY2(QFontDatabase::hasFamily(family), qPrintable(family));
        }
        QVERIFY(families.size() <= QFontDatabase::families().size());
    }

    void theTextFontIsTheAppsFont()
    {
        const QString family = otherFamily();
        if (family.isEmpty())
            QSKIP("Only one font is installed.");
        const QFont desktop = QGuiApplication::font();
        Look look;
        QSignalSpy changed(&look.appearance, &Appearance::changed);
        look.appearance.setTextFont(family);
        QCOMPARE(QGuiApplication::font().family(), family);
        QCOMPARE(look.appearance.textFamily(), family);
        QCOMPARE(look.saved().textFont, family);
        QVERIFY(changed.count() >= 1);

        // the desktop's again
        look.appearance.setTextFont(QString());
        QCOMPARE(QGuiApplication::font(), desktop);
        QCOMPARE(look.saved().textFont, QString());
    }

    void theTextSizeIsTheAppsFontSize()
    {
        Look look;
        const double desktop = look.appearance.desktopTextSize();
        look.appearance.setTextSize(desktop + 3);
        QCOMPARE(QGuiApplication::font().pointSizeF(), desktop + 3);
        QCOMPARE(look.saved().textSize, desktop + 3);

        look.appearance.setTextSize(0);
        QCOMPARE(QGuiApplication::font().pointSizeF(), desktop);
        QCOMPARE(look.saved().textSize, 0.0);
    }

    void theTextSizeStaysReadable()
    {
        Look look;
        look.appearance.setTextSize(200);
        QCOMPARE(look.appearance.textSize(), largestTextSize);
        QCOMPARE(QGuiApplication::font().pointSizeF(), largestTextSize);
        look.appearance.setTextSize(1);
        QCOMPARE(look.appearance.textSize(), smallestTextSize);
        look.appearance.setTextSize(-5);
        QCOMPARE(look.appearance.textSize(), 0.0);
    }

    void aMissingTextFontLeavesTheDesktopsFont()
    {
        const QFont desktop = QGuiApplication::font();
        Look look;
        look.appearance.setTextFont(notInstalled);
        QCOMPARE(QGuiApplication::font(), desktop);
        QCOMPARE(look.appearance.textFamily(), desktop.family());
        QCOMPARE(look.appearance.fontsError(), notInstalled + u" isn't installed, so the default is used."_s);
        // what was chosen stays chosen, for when it's installed
        QCOMPARE(look.appearance.textFont(), notInstalled);

        look.appearance.setTextFont(QString());
        QCOMPARE(look.appearance.fontsError(), QString());
    }

    void theHeadingFontCanBeTheText()
    {
        const QString family = otherFamily();
        if (family.isEmpty())
            QSKIP("Only one font is installed.");
        Look look;
        look.appearance.setHeadingFont(sameAsText.toString());
        QCOMPARE(look.appearance.headingFamily(), look.appearance.textFamily());

        // and moves with it
        look.appearance.setTextFont(family);
        QCOMPARE(look.appearance.headingFamily(), family);
    }

    void theHeadingFontCanBeAFamily()
    {
        const QString family = otherFamily();
        if (family.isEmpty())
            QSKIP("Only one font is installed.");
        Look look;
        look.appearance.setHeadingFont(family);
        QCOMPARE(look.appearance.headingFamily(), family);
        QCOMPARE(look.appearance.fontsError(), QString());
        // and not the text's
        QCOMPARE(look.appearance.textFamily(), QGuiApplication::font().family());
        QVERIFY(look.appearance.textFamily() != family);

        look.appearance.setHeadingFont(QString());
        QCOMPARE(look.appearance.headingFamily(), headingFontFamily());
    }

    void aMissingHeadingFontIsTheAppsOwn()
    {
        Look look;
        look.appearance.setHeadingFont(notInstalled);
        QCOMPARE(look.appearance.headingFamily(), headingFontFamily());
        QCOMPARE(look.appearance.fontsError(), notInstalled + u" isn't installed, so the default is used."_s);
    }

    void saysEveryFontThatIsMissing()
    {
        Look look;
        look.appearance.setHeadingFont(u"Made Up Serif"_s);
        look.appearance.setTextFont(u"Made Up Sans"_s);
        QCOMPARE(look.appearance.fontsError(),
                 u"Made Up Sans, Made Up Serif aren't installed, so the defaults are used."_s);
        // the same one twice is said once
        look.appearance.setHeadingFont(u"Made Up Sans"_s);
        QCOMPARE(look.appearance.fontsError(), u"Made Up Sans isn't installed, so the default is used."_s);
    }

    void readsTheSavedFonts()
    {
        const QString family = otherFamily();
        if (family.isEmpty())
            QSKIP("Only one font is installed.");
        Settings first;
        first.appearance.textFont = family;
        first.appearance.textSize = 13;
        first.appearance.headingFont = sameAsText.toString();
        Look look(first);
        QCOMPARE(QGuiApplication::font().family(), family);
        QCOMPARE(QGuiApplication::font().pointSizeF(), 13.0);
        QCOMPARE(look.appearance.headingFamily(), family);
    }

    void followsTheFontsInTheSettingsFile()
    {
        Look look;
        Settings settings = look.settings.settings();
        settings.appearance.textSize = 15;
        QVERIFY(look.settings.set(settings));
        QCOMPARE(QGuiApplication::font().pointSizeF(), 15.0);
    }

    void putsTheDesktopsFontBack()
    {
        const QFont desktop = QGuiApplication::font();
        {
            Look look;
            look.appearance.setTextSize(look.appearance.desktopTextSize() + 4);
            QVERIFY(QGuiApplication::font() != desktop);
        }
        QCOMPARE(QGuiApplication::font(), desktop);
    }

    void tellsWhenOnlyTheDecorationsChange()
    {
        Look look;
        QSignalSpy changed(&look.appearance, &Appearance::changed);
        look.appearance.setDecorations(u"plain"_s);
        QCOMPARE(changed.count(), 1);
        look.appearance.setDecorations(u"full"_s);
        QCOMPARE(changed.count(), 2);
        // and not for what hasn't changed
        look.appearance.setDecorations(u"full"_s);
        QCOMPARE(changed.count(), 2);
    }

    // The scheme in the source is what the design says it is.
    void theSchemeFileHasTheColors()
    {
        const KSharedConfigPtr config = KSharedConfig::openConfig(colorsFile, KConfig::SimpleConfig);
        QVERIFY(config->hasGroup(u"General"_s));
        QCOMPARE(config->group(u"General"_s).readEntry("Name"), u"Path of Upgrading"_s);

        const auto background = [&](KColorScheme::ColorSet set,
                                    KColorScheme::BackgroundRole role = KColorScheme::NormalBackground) {
            return KColorScheme(QPalette::Active, set, config).background(role).color();
        };
        const auto foreground = [&](KColorScheme::ColorSet set,
                                    KColorScheme::ForegroundRole role = KColorScheme::NormalText) {
            return KColorScheme(QPalette::Active, set, config).foreground(role).color();
        };
        const auto decoration = [&](KColorScheme::DecorationRole role) {
            return KColorScheme(QPalette::Active, KColorScheme::Window, config).decoration(role).color();
        };

        QCOMPARE(background(KColorScheme::Window), QColor(u"#100e0c"_s));
        QCOMPARE(background(KColorScheme::View), QColor(u"#1a1612"_s));
        QCOMPARE(background(KColorScheme::View, KColorScheme::AlternateBackground), QColor(u"#211c17"_s));
        QCOMPARE(background(KColorScheme::Header), QColor(u"#15120f"_s));
        QCOMPARE(background(KColorScheme::Button), QColor(u"#2a231c"_s));
        QCOMPARE(background(KColorScheme::Selection), QColor(u"#b8924a"_s));
        QCOMPARE(background(KColorScheme::Tooltip), QColor(u"#1f1a15"_s));
        QCOMPARE(background(KColorScheme::Complementary), QColor(u"#0c0a08"_s));

        QCOMPARE(foreground(KColorScheme::Window), QColor(u"#ddd2c2"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::InactiveText), QColor(u"#948776"_s));
        QCOMPARE(foreground(KColorScheme::Selection), QColor(u"#120f0c"_s));
        QCOMPARE(foreground(KColorScheme::Complementary), QColor(u"#e6dccd"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::LinkText), QColor(u"#d4b06a"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::VisitedText), QColor(u"#a8874a"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::PositiveText), QColor(u"#7fb069"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::NegativeText), QColor(u"#c8553d"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::NeutralText), QColor(u"#d9a441"_s));
        QCOMPARE(decoration(KColorScheme::FocusColor), QColor(u"#b8924a"_s));
        QCOMPARE(decoration(KColorScheme::HoverColor), QColor(u"#d4b06a"_s));
    }

    void everySetIsFilledIn()
    {
        const KSharedConfigPtr config = KSharedConfig::openConfig(colorsFile, KConfig::SimpleConfig);
        const QStringList sets = {u"Button"_s,  u"Complementary"_s, u"Header"_s, u"Selection"_s,
                                  u"Tooltip"_s, u"View"_s,          u"Window"_s};
        const QStringList keys = {u"BackgroundAlternate"_s, u"BackgroundNormal"_s,   u"DecorationFocus"_s,
                                  u"DecorationHover"_s,     u"ForegroundActive"_s,   u"ForegroundInactive"_s,
                                  u"ForegroundLink"_s,      u"ForegroundNegative"_s, u"ForegroundNeutral"_s,
                                  u"ForegroundNormal"_s,    u"ForegroundPositive"_s, u"ForegroundVisited"_s};
        for (const QString &set : sets) {
            const KConfigGroup group = config->group(u"Colors:"_s + set);
            for (const QString &key : keys) {
                QVERIFY2(group.hasKey(key), qPrintable(set + u"/"_s + key));
                // a color is three numbers
                QCOMPARE(group.readEntry(key, QString()).split(u',').size(), 3);
            }
        }
        QVERIFY(config->hasGroup(u"ColorEffects:Disabled"_s));
        QVERIFY(config->hasGroup(u"ColorEffects:Inactive"_s));
    }

    // Kirigami gets its colors from the app's palette. This is the only engine
    // in the program, as a second one doesn't follow later changes.
    void kirigamiFollowsTheColors()
    {
        Look look;
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nimport org.kde.kirigami as Kirigami\n"
                          "Item { readonly property color background: Kirigami.Theme.backgroundColor }",
                          QUrl());
        std::unique_ptr<QObject> made(component.create());
        QVERIFY2(made, qPrintable(component.errorString()));
        // in a window, as Kirigami's colors come with one
        QQuickWindow window;
        auto *item = qobject_cast<QQuickItem *>(made.get());
        item->setParentItem(window.contentItem());
        // the palette is passed on when the event loop gets to it
        QTRY_COMPARE(item->property("background").value<QColor>(), appWindow);

        look.appearance.setColors(u"desktop"_s);
        QTRY_VERIFY(item->property("background").value<QColor>() != appWindow);
        look.appearance.setColors(u"app"_s);
        QTRY_COMPARE(item->property("background").value<QColor>(), appWindow);
    }
};

QTEST_MAIN(TestAppearance)
#include "tst_appearance.moc"
