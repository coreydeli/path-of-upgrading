// SPDX-License-Identifier: MIT
#include "app/appearance.h"
#include "app/fonts.h"
#include "app/settingsstore.h"
#include "stylefortests.h"

#include <KColorScheme>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
#include <QStyleHints>
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

const QColor appWindow = QColor(u"#0c0c0d"_s);

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
        // the app's own controls, as the program has
        useStyleInTests();
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    }

private Q_SLOTS:
    void initTestCase()
    {
        // so nothing of the desktop's colors is read or written
        QStandardPaths::setTestModeEnabled(true);
        // a container can have no fonts at all, and the font pickers need one
        QVERIFY(addBundledFonts());
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
                 QColor(u"#35b5a8"_s));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::WindowText),
                 QColor(u"#ece6da"_s));
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

    void fontsStartAsTheApps()
    {
        const QFont desktop = QGuiApplication::font();
        Look look;
        QCOMPARE(look.appearance.headingFont(), QString());
        QCOMPARE(look.appearance.textFont(), QString());
        QCOMPARE(look.appearance.textSize(), 0.0);
        QCOMPARE(look.appearance.headingFamily(), headingFontFamily());
        QCOMPARE(look.appearance.textFamily(), textFontFamily());
        QCOMPARE(QGuiApplication::font().family(), u"Spectral"_s);
        QCOMPARE(look.appearance.desktopTextSize(), desktop.pointSizeF());
        QCOMPARE(QGuiApplication::font().pointSizeF(), desktop.pointSizeF());
        QCOMPARE(look.appearance.fontsError(), QString());
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
        Look look;
        QSignalSpy changed(&look.appearance, &Appearance::changed);
        look.appearance.setTextFont(family);
        QCOMPARE(QGuiApplication::font().family(), family);
        QCOMPARE(look.appearance.textFamily(), family);
        QCOMPARE(look.saved().textFont, family);
        QVERIFY(changed.count() >= 1);

        // the default again
        look.appearance.setTextFont(QString());
        QCOMPARE(QGuiApplication::font().family(), textFontFamily());
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

    void aMissingTextFontLeavesTheDefault()
    {
        Look look;
        look.appearance.setTextFont(notInstalled);
        QCOMPARE(QGuiApplication::font().family(), textFontFamily());
        QCOMPARE(look.appearance.textFamily(), textFontFamily());
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

    void startsInDossier()
    {
        Look look;
        QCOMPARE(look.appearance.design(), u"dossier"_s);
        QCOMPARE(look.appearance.brightness(), u"desktop"_s);
        QVERIFY(!look.appearance.isMaterial());
        QVERIFY(look.appearance.isDark());
    }

    void savesTheDesign()
    {
        Look look;
        look.appearance.setDesign(u"material"_s);
        look.appearance.setBrightness(u"dark"_s);
        QCOMPARE(look.saved().design, Design::Material);
        QCOMPARE(look.saved().brightness, Brightness::Dark);
        QVERIFY(look.appearance.isMaterial());

        QSettings again(look.dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        QCOMPARE(again.value(u"appearance/design"_s).toString(), u"material"_s);
        QCOMPARE(again.value(u"appearance/brightness"_s).toString(), u"dark"_s);

        look.appearance.setDesign(u"neon"_s);
        QCOMPARE(look.appearance.design(), u"dossier"_s);
    }

    void materialHasItsOwnLightAndDarkColors()
    {
        Settings first;
        first.appearance.design = Design::Material;
        first.appearance.brightness = Brightness::Light;
        Look look(first);
        QVERIFY(look.appearance.hasAppColors());
        QVERIFY(!look.appearance.isDark());
        QCOMPARE(window(), QColor(u"#fef7ff"_s));
        QVERIFY(qApp->property("KDE_COLOR_SCHEME_PATH").toString().endsWith(u"MaterialLight.colors"_s));

        look.appearance.setBrightness(u"dark"_s);
        QVERIFY(look.appearance.isDark());
        QCOMPARE(window(), QColor(u"#141218"_s));
        QVERIFY(qApp->property("KDE_COLOR_SCHEME_PATH").toString().endsWith(u"MaterialDark.colors"_s));

        // and the one the app had, when it goes back
        look.appearance.setDesign(u"dossier"_s);
        QCOMPARE(window(), appWindow);
        QVERIFY(look.appearance.isDark());
    }

    void materialFollowsTheDesktopsBrightnessByDefault()
    {
        Settings first;
        first.appearance.design = Design::Material;
        Look look(first);
        // there's no desktop offscreen, so the scheme isn't known and it's light
        QCOMPARE(look.appearance.isDark(),
                 QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    }

    void tellsWhenTheBrightnessChanges()
    {
        Settings first;
        first.appearance.design = Design::Material;
        first.appearance.brightness = Brightness::Light;
        Look look(first);
        QSignalSpy changed(&look.appearance, &Appearance::changed);
        look.appearance.setBrightness(u"dark"_s);
        QVERIFY(changed.count() >= 1);
    }

    void materialKeepsTheDesktopsFont()
    {
        const QFont desktop = QGuiApplication::font();
        Settings first;
        first.appearance.design = Design::Material;
        Look look(first);
        QCOMPARE(look.appearance.textFamily(), desktop.family());
        QCOMPARE(look.appearance.headingFamily(), desktop.family());

        // and a font chosen is still the one it has
        const QString other = otherFamily();
        if (other.isEmpty())
            QSKIP("There's no other font installed.");
        look.appearance.setTextFont(other);
        QCOMPARE(look.appearance.textFamily(), other);
    }

    void aMissingMaterialSchemeLeavesTheDesktopsColors()
    {
        const QColor before = window();
        Settings first;
        first.appearance.design = Design::Material;
        Look look(first, u"/made-up/color-schemes/PathOfUpgrading.colors"_s);
        QVERIFY(!look.appearance.hasAppColors());
        QVERIFY(!look.appearance.colorsError().isEmpty());
        QCOMPARE(window(), before);
    }

    // every look that has a light and a dark has both files
    void theLooksSchemesAreFilledIn()
    {
        const QStringList sets = {u"Button"_s,  u"Complementary"_s, u"Header"_s, u"Selection"_s,
                                  u"Tooltip"_s, u"View"_s,          u"Window"_s};
        int checked = 0;
        for (const DesignInfo &info : designs()) {
            if (info.schemeName.isEmpty())
                continue;
            for (const QString &brightness : {u"Light"_s, u"Dark"_s}) {
                const QString name = info.schemeName + brightness;
                const QString file =
                    QFileInfo(colorsFile).dir().filePath(u"PathOfUpgrading"_s + name + u".colors"_s);
                const KSharedConfigPtr config = KSharedConfig::openConfig(file, KConfig::SimpleConfig);
                QVERIFY2(config->hasGroup(u"General"_s), qPrintable(file));
                for (const QString &set : sets) {
                    const KConfigGroup group = config->group(u"Colors:"_s + set);
                    for (const char *key : {"BackgroundNormal", "BackgroundAlternate", "ForegroundNormal",
                                            "ForegroundInactive", "DecorationFocus", "DecorationHover"})
                        QVERIFY2(group.hasKey(key), qPrintable(name + u"/"_s + set));
                }
                ++checked;
            }
        }
        QVERIFY(checked >= 2);
    }

    void listsEveryLookForThePickers()
    {
        Look look;
        const QVariantList list = look.appearance.designList();
        QCOMPARE(list.size(), designs().size());
        for (int i = 0; i < list.size(); ++i) {
            const QVariantMap entry = list.at(i).toMap();
            QCOMPARE(entry.value(u"id"_s).toString(), designs().at(i).id);
            QCOMPARE(entry.value(u"name"_s).toString(), designs().at(i).name);
            QCOMPARE(entry.value(u"swatch"_s).toStringList().size(), 4);
        }
    }

    void onlyTheLooksWithALightPaletteHaveABrightness()
    {
        Settings first;
        first.appearance.brightness = Brightness::Light;
        Look dossier(first);
        QVERIFY(!dossier.appearance.hasLight());
        // always dark, whatever was chosen
        QVERIFY(dossier.appearance.isDark());

        first.appearance.design = Design::Material;
        Look material(first);
        QVERIFY(material.appearance.hasLight());
        QVERIFY(!material.appearance.isDark());
    }

    void aLooksOwnFontsComeFromItsTable()
    {
        Look dossier;
        QCOMPARE(dossier.appearance.designHeadingFont(), u"Big Shoulders Display"_s);
        QCOMPARE(dossier.appearance.designTextFont(), u"Spectral"_s);
        dossier.appearance.setDesign(u"material"_s);
        QCOMPARE(dossier.appearance.designHeadingFont(), QString());
        QCOMPARE(dossier.appearance.designTextFont(), QString());
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

        QCOMPARE(background(KColorScheme::Window), QColor(u"#0c0c0d"_s));
        QCOMPARE(background(KColorScheme::View), QColor(u"#161618"_s));
        QCOMPARE(background(KColorScheme::View, KColorScheme::AlternateBackground), QColor(u"#1e1e21"_s));
        QCOMPARE(background(KColorScheme::Header), QColor(u"#080809"_s));
        QCOMPARE(background(KColorScheme::Button), QColor(u"#161618"_s));
        QCOMPARE(background(KColorScheme::Selection), QColor(u"#35b5a8"_s));
        QCOMPARE(background(KColorScheme::Tooltip), QColor(u"#ece6da"_s));
        QCOMPARE(background(KColorScheme::Complementary), QColor(u"#0b0b0c"_s));

        QCOMPARE(foreground(KColorScheme::Window), QColor(u"#ece6da"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::InactiveText), QColor(u"#969086"_s));
        QCOMPARE(foreground(KColorScheme::Selection), QColor(u"#0c0c0d"_s));
        QCOMPARE(foreground(KColorScheme::Complementary), QColor(u"#ece6da"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::LinkText), QColor(u"#35b5a8"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::VisitedText), QColor(u"#969086"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::PositiveText), QColor(u"#70c46c"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::NegativeText), QColor(u"#e84e50"_s));
        QCOMPARE(foreground(KColorScheme::Window, KColorScheme::NeutralText), QColor(u"#f0b63e"_s));
        QCOMPARE(decoration(KColorScheme::FocusColor), QColor(u"#35b5a8"_s));
        QCOMPARE(decoration(KColorScheme::HoverColor), QColor(u"#5acdc0"_s));
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
