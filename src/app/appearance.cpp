// SPDX-License-Identifier: MIT
#include "app/appearance.h"

#include "app/settingsstore.h"

#include <KColorSchemeManager>
#include <QDir>
#include <QFileInfo>
#include <QModelIndex>

using namespace Qt::Literals::StringLiterals;

namespace pou {

Appearance::Appearance(SettingsStore *settings, const QString &schemeFile, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_schemeFile(schemeFile)
{
    connect(settings, &SettingsStore::changed, this, &Appearance::follow);
    follow();
}

QString Appearance::schemeId()
{
    return u"PathOfUpgrading"_s;
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

void Appearance::follow()
{
    const bool wasApp = m_appColors;
    const QString hadError = m_error;
    if (appearance().colors == ColorTheme::App) {
        m_appColors = activateScheme();
    } else {
        m_error.clear();
        if (m_appColors) {
            // an invalid index is the desktop's own scheme
            m_manager->activateScheme({});
            m_appColors = false;
        }
    }
    if (m_appColors != wasApp || m_error != hadError)
        Q_EMIT changed();
}

bool Appearance::activateScheme()
{
    if (m_appColors)
        return true;
    m_error.clear();
    const QFileInfo file(m_schemeFile);
    if (!file.exists()) {
        m_error = u"The app's colors are missing, so it has your desktop's."_s;
        return false;
    }
    if (!m_manager) {
        // The manager looks for color-schemes in the data folders once, when
        // it's made. The app's is in a folder of its own so System Settings
        // doesn't list it, so that folder is one of them for now.
        const QString parent = file.dir().absolutePath() + u"/.."_s;
        const bool hadFolders = qEnvironmentVariableIsSet("XDG_DATA_DIRS");
        const QByteArray folders = qgetenv("XDG_DATA_DIRS");
        const QByteArray usual = hadFolders ? folders : QByteArrayLiteral("/usr/local/share:/usr/share");
        QByteArray withApp = QDir::cleanPath(parent).toLocal8Bit();
        withApp += ':';
        withApp += usual;
        qputenv("XDG_DATA_DIRS", withApp);
        m_manager = KColorSchemeManager::instance();
        if (hadFolders)
            qputenv("XDG_DATA_DIRS", folders);
        else
            qunsetenv("XDG_DATA_DIRS");
        // it would write the choice to the app's own settings file
        m_manager->setAutosaveChanges(false);
    }
    const QModelIndex index = m_manager->indexForSchemeId(schemeId());
    if (!index.isValid()) {
        m_error = u"The app's colors couldn't be loaded, so it has your desktop's."_s;
        return false;
    }
    m_manager->activateScheme(index);
    return true;
}

} // namespace pou
