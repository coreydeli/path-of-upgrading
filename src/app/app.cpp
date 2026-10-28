// SPDX-License-Identifier: MIT
#include "app/app.h"

#include "platform/gameinput.h"
#include "platform/globalshortcuts.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace pou {

App::Paths App::defaultPaths()
{
    // ~/.config/path-of-upgradingrc, ~/.local/share/path-of-upgrading and
    // ~/.cache/path-of-upgrading
    return {
        .settings = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' +
                    QCoreApplication::applicationName() + u"rc"_s,
        .data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        .cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
    };
}

App::App(QQmlEngine *qml, std::unique_ptr<GameInput> input, std::unique_ptr<GlobalShortcuts> shortcuts,
         std::unique_ptr<QNetworkAccessManager> network, const PobPlaces &places, const Paths &paths,
         QObject *parent)
    : QObject(parent)
    , m_places(places)
    , m_settings(paths.settings)
    , m_characters(paths.data + u"/characters"_s)
    , m_character(&m_characters, &m_pob)
    , m_input(std::move(input))
    , m_checker(qml, m_input.get())
    , m_shortcuts(std::move(shortcuts))
    , m_hotkeys(m_shortcuts.get())
    , m_history(paths.data + u"/history.json"_s)
    , m_network(std::move(network))
    , m_tradeClient(m_network.get())
    , m_tradeData(&m_tradeClient, paths.cache + u"/trade"_s)
    , m_rates(m_network.get(), paths.cache + u"/rates"_s)
    , m_trade(&m_tradeClient)
    , m_finder(&m_trade, &m_tradeData, &m_rates)
    , m_characterList(&m_characters, &m_character, &m_settings)
    , m_buildImporter(&m_characterList, &m_pob, m_network.get())
    , m_gearList(&m_character, &m_settings)
    , m_tradeOptions(&m_settings, &m_tradeData, &m_rates)
    , m_overlayOptions(&m_settings, &m_hotkeys)
    , m_pobOptions(&m_settings, &m_pob)
{
    m_characterList.load();
    connect(&m_character, &ActiveCharacter::changed, this, [this] {
        m_checker.setCharacter(m_character.character(), m_character.build());
        m_finder.setCharacter(m_character.character());
    });
    connect(&m_settings, &SettingsStore::changed, this, &App::followSettings);

    const Settings &settings = m_settings.settings();
    m_character.setId(settings.character);
    lookForPob();
    m_checker.setPob(&m_pob);
    m_checker.setFinder(&m_finder);
    m_checker.setSettings(settings);
    connect(&m_checker, &ItemChecker::itemRead, &m_history, &ItemHistory::add);

    // the league searches go to can change with the site's leagues, and the
    // rates are the league's
    connect(&m_tradeData, &TradeDataStore::refreshed, this, [this] {
        m_trade.setLeagues(m_tradeData.leagues());
        if (m_trade.unavailable().isEmpty())
            m_rates.refresh(m_trade.league().tradeId());
    });
    followTrade();

    connect(&m_hotkeys, &Hotkeys::pressed, this, [this](Hotkeys::Hotkey hotkey) {
        if (hotkey == Hotkeys::Hotkey::Check)
            m_checker.check();
    });
    connect(&m_hotkeys, &Hotkeys::keyChanged, this, &App::followKeys);
    m_hotkeys.add(settings.overlay);
    // which have the keys they had last time, when they've been changed on
    // the desktop since
    followKeys();
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
    followTrade();
}

void App::followKeys()
{
    // without the desktop's shortcuts, they have none
    if (!m_hotkeys.isAvailable())
        return;
    Settings settings = m_settings.settings();
    settings.overlay.hotkey = m_hotkeys.key(Hotkeys::Hotkey::Check);
    settings.overlay.resultsHotkey = m_hotkeys.key(Hotkeys::Hotkey::Results);
    m_settings.set(settings);
}

void App::followTrade()
{
    m_trade.setSettings(m_settings.settings().trade);
    m_finder.setSettings(m_settings.settings());
    m_checker.popup().setTradeAvailable(m_trade.unavailable().isEmpty());
    // which does nothing while what there is is fresh
    if (m_trade.unavailable().isEmpty())
        m_tradeData.refresh();
}

} // namespace pou
