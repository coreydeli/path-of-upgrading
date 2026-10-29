// SPDX-License-Identifier: MIT
#include "app/app.h"

#include "app/commandline.h"
#include "platform/gameinput.h"
#include "platform/globalshortcuts.h"
#include "platform/secretstore.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace pou {

App::Paths App::defaultPaths()
{
    // ~/.config/path-of-upgradingrc, ~/.local/share/path-of-upgrading,
    // ~/.cache/path-of-upgrading and the entry in ~/.config/autostart
    return {
        .settings = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' +
                    QCoreApplication::applicationName() + u"rc"_s,
        .data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        .cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
        .autostart = Autostart::fileFor(QGuiApplication::desktopFileName()),
    };
}

App::App(QQmlEngine *qml, std::unique_ptr<GameInput> input, std::unique_ptr<GlobalShortcuts> shortcuts,
         std::unique_ptr<SecretStore> secrets, std::unique_ptr<QNetworkAccessManager> network,
         const PobPlaces &places, const Paths &paths, QObject *parent)
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
    , m_secrets(std::move(secrets))
    , m_network(std::move(network))
    , m_tradeClient(m_network.get())
    , m_tradeSession(m_secrets.get(), &m_tradeClient)
    , m_tradeData(&m_tradeClient, paths.cache + u"/trade"_s)
    , m_rates(m_network.get(), paths.cache + u"/rates"_s)
    , m_trade(&m_tradeClient)
    , m_finder(&m_trade, &m_tradeData, &m_rates)
    , m_results(&m_trade, &m_rates)
    , m_resultsPanel(qml, m_input.get(), &m_results)
    , m_characterList(&m_characters, &m_character, &m_settings)
    , m_buildImporter(&m_characterList, &m_pob, m_network.get())
    , m_gearList(&m_character, &m_settings)
    , m_gearScoring(&m_character)
    , m_statWeights(&m_character)
    , m_statPicker(&m_tradeData, &m_history)
    , m_ignoredStats(&m_character)
    , m_checkedItems(&m_history)
    , m_tradeOptions(&m_settings, &m_tradeData, &m_rates)
    , m_autostart(paths.autostart, {.name = QGuiApplication::applicationDisplayName(),
                                    .exec = CommandLine::backgroundCommand(),
                                    .icon = QGuiApplication::desktopFileName()})
    , m_overlayOptions(&m_settings, &m_hotkeys, &m_autostart)
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
    m_resultsPanel.setSettings(settings.overlay);
    connect(&m_checker, &ItemChecker::itemRead, &m_history, &ItemHistory::add);
    connect(&m_checkedItems, &CheckedItems::checkAgainAsked, &m_checker, &ItemChecker::checkItem);
    connect(&m_checker, &ItemChecker::wearAsked, this, [this](const Item &item, const QString &slot) {
        m_character.wear(slot, item);
        m_checker.popup().setWearError(m_character.errorString());
    });

    // the league searches go to can change with the site's leagues, and the
    // rates are the league's
    connect(&m_tradeData, &TradeDataStore::refreshed, this, [this] {
        m_trade.setLeagues(m_tradeData.leagues());
        if (m_trade.unavailable().isEmpty())
            m_rates.refresh(m_trade.league().tradeId());
    });
    // what a search finds is what the results have, and they ask the site
    // only once you want to see them
    connect(&m_finder, &TradeFinder::finished, this, [this] {
        if (const std::optional<TradeFinder::Found> &found = m_finder.found())
            m_results.setSearch(*found);
    });
    // the site takes bigger searches signed in, so what it limited them to
    // before isn't kept
    connect(&m_tradeSession, &TradeSession::idChanged, &m_finder, &TradeFinder::forgetSize);
    followTrade();

    connect(&m_hotkeys, &Hotkeys::pressed, this, [this](Hotkeys::Hotkey hotkey) {
        if (hotkey == Hotkeys::Hotkey::Check)
            m_checker.check();
        else if (hotkey == Hotkeys::Hotkey::Results && !m_settings.settings().overlay.paused)
            m_resultsPanel.toggle();
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
    m_resultsPanel.setSettings(settings.overlay);
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
    m_resultsPanel.setUnavailable(m_trade.unavailable());
    m_statPicker.setSiteAllowed(m_trade.unavailable().isEmpty());
    if (m_trade.unavailable().isEmpty()) {
        // which does nothing while what there is is fresh
        m_tradeData.refresh();
        // and this asks the keyring once
        m_tradeSession.load();
    }
}

} // namespace pou
