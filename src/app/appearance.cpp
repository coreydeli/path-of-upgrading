// SPDX-License-Identifier: MIT
#include "app/appearance.h"

#include "app/fonts.h"
#include "app/settingsstore.h"

#include <KColorScheme>
#include <KColorSchemeManager>
#include <KSharedConfig>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QModelIndex>
#include <QStyleHints>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {

Appearance::Appearance(SettingsStore *settings, const QString &schemeFile, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_schemeFile(schemeFile)
    , m_desktopFont(QGuiApplication::font())
    , m_font(m_desktopFont)
{
    connect(settings, &SettingsStore::changed, this, &Appearance::follow);
    // Material's colors follow the desktop's light or dark
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &Appearance::follow);
    follow();
}

Appearance::~Appearance()
{
    if (QGuiApplication::font() != m_desktopFont)
        QGuiApplication::setFont(m_desktopFont);
}

const AppearanceSettings &Appearance::appearance() const
{
    return m_settings->settings().appearance;
}

template<typename Edit>
void Appearance::change(Edit edit)
{
    Settings settings = m_settings->settings();
    edit(settings.appearance);
    // which does nothing when it's the same
    m_settings->set(settings);
}

QString Appearance::colors() const
{
    return colorThemeId(appearance().colors);
}

void Appearance::setColors(const QString &colors)
{
    change([&](AppearanceSettings &appearance) { appearance.colors = colorThemeFromId(colors); });
}

QString Appearance::decorations() const
{
    return decorationsId(appearance().decorations);
}

void Appearance::setDecorations(const QString &decorations)
{
    change([&](AppearanceSettings &appearance) { appearance.decorations = decorationsFromId(decorations); });
}

QString Appearance::design() const
{
    return designId(appearance().design);
}

void Appearance::setDesign(const QString &design)
{
    change([&](AppearanceSettings &appearance) { appearance.design = designFromId(design); });
}

QString Appearance::brightness() const
{
    return brightnessId(appearance().brightness);
}

void Appearance::setBrightness(const QString &brightness)
{
    change([&](AppearanceSettings &appearance) { appearance.brightness = brightnessFromId(brightness); });
}

bool Appearance::isDark() const
{
    if (!isMaterial())
        return true;
    switch (appearance().brightness) {
    case Brightness::Light:
        return false;
    case Brightness::Dark:
        return true;
    case Brightness::Desktop:
        break;
    }
    // an unknown scheme, as offscreen has, is light
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void Appearance::setHeadingFont(const QString &family)
{
    change([&](AppearanceSettings &appearance) { appearance.headingFont = family; });
}

void Appearance::setTextFont(const QString &family)
{
    change([&](AppearanceSettings &appearance) { appearance.textFont = family; });
}

void Appearance::setTextSize(double points)
{
    // 0 is the desktop's, and the rest stays where it's readable
    if (points > 0)
        points = std::clamp(points, smallestTextSize, largestTextSize);
    change([&](AppearanceSettings &appearance) { appearance.textSize = std::max(points, 0.0); });
}

QStringList Appearance::families() const
{
    return scalableFamilies();
}

void Appearance::follow()
{
    const bool wasApp = m_appColors;
    const bool wasDark = m_dark;
    const QString hadError = m_error;
    m_dark = isDark();
    if (appearance().colors == ColorTheme::App) {
        m_appColors = activateScheme();
    } else {
        m_error.clear();
        if (m_appColors) {
            if (!m_manager) {
                m_manager = KColorSchemeManager::instance();
                // it would write the choice to the app's own settings file
                m_manager->setAutosaveChanges(false);
            }
            // an invalid index is the desktop's own scheme
            m_manager->activateScheme({});
            m_appColors = false;
            m_activeScheme.clear();
        }
    }
    followFonts();
    // Kirigami's corners are round in Material
    qApp->setProperty("POU_ROUND_CORNERS", isMaterial());
    // what the colors and fonts came to, and the choices that go with them
    const bool different = appearance() != m_seen;
    m_seen = appearance();
    if (m_appColors != wasApp || m_error != hadError || m_dark != wasDark || different)
        Q_EMIT changed();
}

void Appearance::followFonts()
{
    const AppearanceSettings &chosen = appearance();
    QStringList missing;

    // The app's own face unless another is chosen, and Material has the
    // desktop's.
    QFont font = m_desktopFont;
    const QString standard = isMaterial() ? m_desktopFont.family() : textFontFamily();
    QString family = chosen.textFont.isEmpty() ? standard : chosen.textFont;
    if (!QFontDatabase::hasFamily(family)) {
        if (!chosen.textFont.isEmpty())
            missing << chosen.textFont;
        family = standard;
    }
    if (QFontDatabase::hasFamily(family))
        font.setFamily(family);
    if (chosen.textSize > 0)
        font.setPointSizeF(chosen.textSize);
    if (font != m_font) {
        m_font = font;
        QGuiApplication::setFont(font);
    }

    if (chosen.headingFont.isEmpty()) {
        m_headingFamily = isMaterial() ? font.family() : headingFontFamily();
    } else if (chosen.headingFont == sameAsText) {
        m_headingFamily = font.family();
    } else if (QFontDatabase::hasFamily(chosen.headingFont)) {
        m_headingFamily = chosen.headingFont;
    } else {
        m_headingFamily = isMaterial() ? font.family() : headingFontFamily();
        missing << chosen.headingFont;
    }

    m_fontsError.clear();
    if (!missing.isEmpty()) {
        missing.removeDuplicates();
        m_fontsError =
            missing.join(u", "_s) + (missing.size() == 1 ? u" isn't installed, so the default is used."_s
                                                         : u" aren't installed, so the defaults are used."_s);
    }
}

QString Appearance::schemeFile() const
{
    if (!isMaterial())
        return m_schemeFile;
    const QString name =
        m_dark ? u"PathOfUpgradingMaterialDark.colors"_s : u"PathOfUpgradingMaterialLight.colors"_s;
    return QFileInfo(m_schemeFile).dir().filePath(name);
}

bool Appearance::activateScheme()
{
    const QString file = schemeFile();
    if (m_appColors && file == m_activeScheme)
        return true;
    m_error.clear();
    if (!QFileInfo::exists(file)) {
        m_error = u"The app's colors are missing, so it has your desktop's."_s;
        return false;
    }
    // The color scheme manager only knows the schemes in the data folders, and
    // the app's isn't to be listed in System Settings, so the palette is made
    // from the file.
    const KSharedConfigPtr config = KSharedConfig::openConfig(file, KConfig::SimpleConfig);
    if (!config->hasGroup(u"Colors:Window"_s)) {
        m_error = u"The app's colors couldn't be loaded, so it has your desktop's."_s;
        return false;
    }
    // The Plasma style takes its colors from the scheme this names, not from
    // the palette. It's what the color scheme manager sets too.
    qApp->setProperty("KDE_COLOR_SCHEME_PATH", file);
    QGuiApplication::setPalette(KColorScheme::createApplicationPalette(config));
    m_activeScheme = file;
    return true;
}

} // namespace pou
