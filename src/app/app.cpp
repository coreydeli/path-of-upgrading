// SPDX-License-Identifier: MIT
#include "app/app.h"

#include "platform/gameinput.h"

#include <QCoreApplication>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace pou {

App::Paths App::defaultPaths()
{
    // ~/.config/path-of-upgradingrc and ~/.local/share/path-of-upgrading
    return {
        .settings = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' +
                    QCoreApplication::applicationName() + u"rc"_s,
        .data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
    };
}

App::App(QQmlEngine *qml, std::unique_ptr<GameInput> input, const PobPlaces &places, const Paths &paths,
         QObject *parent)
    : QObject(parent)
    , m_places(places)
    , m_settings(paths.settings)
    , m_characters(paths.data + u"/characters"_s)
    , m_character(&m_characters, &m_pob)
    , m_input(std::move(input))
    , m_checker(qml, m_input.get())
{
    m_characters.load();
    connect(&m_character, &ActiveCharacter::changed, this,
            [this] { m_checker.setCharacter(m_character.character(), m_character.build()); });
    connect(&m_settings, &SettingsStore::changed, this, &App::followSettings);

    const Settings &settings = m_settings.settings();
    m_character.setId(settings.character);
    lookForPob();
    m_checker.setPob(&m_pob);
    m_checker.setSettings(settings);
}

App::~App() = default;

void App::lookForPob()
{
    m_pobLocation = m_settings.settings().pob.location;
    QString why;
    if (const std::optional<PobInstall> install = findPob(m_places, m_pobLocation, &why))
        m_pob.setInstall(*install);
    else
        m_pob.setUnavailable(why);
}

void App::followSettings()
{
    const Settings &settings = m_settings.settings();
    m_checker.setSettings(settings);
    if (settings.character != m_character.id())
        m_character.setId(settings.character);
    if (settings.pob.location != m_pobLocation)
        lookForPob();
}

} // namespace pou
