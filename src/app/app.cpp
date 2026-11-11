// SPDX-License-Identifier: MIT
#include "app/app.h"

#include "app/commandline.h"
#include "platform/gameinput.h"
#include "platform/gamewatcher.h"
#include "platform/globalshortcuts.h"
#include "platform/opener.h"
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
    //
    // The color scheme is in the data folders, and a build folder run can
    // name the one in the source.
    QString colors = qEnvironmentVariable("PATH_OF_UPGRADING_COLORS");
    if (colors.isEmpty()) {
#ifdef Q_OS_WIN
        // where KDE's programs on Windows have theirs, in the data folder next to the program
        const QString relative = u"color-schemes/PathOfUpgrading.colors"_s;
#else
        const QString relative = u"path-of-upgrading/color-schemes/PathOfUpgrading.colors"_s;
#endif
        colors = QStandardPaths::locate(QStandardPaths::GenericDataLocation, relative);
    }
    return {
        .settings = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' +
                    QCoreApplication::applicationName() + u"rc"_s,
        .data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        .cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
        .autostart = Autostart::fileFor(QGuiApplication::desktopFileName()),
        .colors = colors,
    };
}

App::App(QQmlEngine *qml, std::unique_ptr<GameInput> input, std::unique_ptr<GlobalShortcuts> shortcuts,
         std::unique_ptr<SecretStore> secrets, std::unique_ptr<Opener> opener,
         std::unique_ptr<QNetworkAccessManager> network, const PobPlaces &places, const Paths &paths,
         std::unique_ptr<GameWatcher> game, QObject *parent)
    : QObject(parent)
    , m_places(places)
    , m_tradeOffered(paths.tradeOffered)
    , m_mapsOffered(paths.mapsOffered)
    , m_settings(paths.settings)
    , m_characters(paths.data + u"/characters"_s)
    , m_character(&m_characters, &m_pob)
    , m_input(std::move(input))
    , m_checker(qml, m_input.get())
    , m_weigher(&m_character, &m_pob)
    , m_shortcuts(std::move(shortcuts))
    , m_game(std::move(game))
    , m_hotkeys(m_shortcuts.get(), m_game.get())
    , m_history(paths.data + u"/history.json"_s)
    , m_secrets(std::move(secrets))
    , m_opener(std::move(opener))
    , m_network(std::move(network))
    , m_tradeClient(m_network.get())
    , m_tradeSession(m_secrets.get(), &m_tradeClient, m_opener.get())
    , m_tradeData(&m_tradeClient, paths.cache + u"/trade"_s)
    , m_rates(m_network.get(), paths.cache + u"/rates"_s)
    , m_ratesRefresher(&m_rates)
    , m_trade(&m_tradeClient)
    , m_finder(&m_trade, &m_tradeData, &m_rates)
    , m_results(&m_trade, &m_rates)
    , m_resultsPanel(qml, m_input.get(), &m_results)
    , m_updates(m_network.get())
    , m_updatesModel(&m_updates, &m_settings, m_opener.get())
    , m_characterList(&m_characters, &m_character, &m_settings)
    , m_buildImporter(&m_characterList, &m_pob, m_network.get())
    , m_gearList(&m_character, &m_settings)
    , m_upgrades(&m_character, &m_settings, &m_trade, &m_tradeData, &m_rates, &m_results)
    , m_resistances(&m_character)
    , m_gearScoring(&m_character, &m_weigher)
    , m_statWeights(&m_character)
    , m_statPicker(&m_tradeData, &m_history)
    , m_ignoredStats(&m_character)
    , m_checkedItems(&m_history)
    , m_tradeOptions(&m_settings, &m_tradeData, &m_rates)
    , m_autostart(paths.autostart, {.name = QGuiApplication::applicationDisplayName(),
                                    .exec = CommandLine::backgroundCommand(),
                                    .icon = QGuiApplication::desktopFileName()})
    , m_overlayOptions(&m_settings, &m_hotkeys, &m_autostart)
    , m_pobOptions(&m_settings, &m_pob, m_places, m_opener.get())
    , m_about(&m_pobOptions, m_opener.get())
    , m_help(m_opener.get(), paths.tradeOffered)
    , m_appearance(&m_settings, paths.colors)
    , m_onboarding(&m_settings, &m_characterList, &m_overlayOptions)
{
    m_characterList.load();
    // which needs the characters
    m_onboarding.decide();
    // so you can wear gear and set weights before adding a character
    m_character.setStarter([this](QString *why) {
        const QString id = m_characterList.add(u"My character"_s);
        if (id.isEmpty())
            *why = m_characterList.error();
        return id;
    });
    connect(&m_character, &ActiveCharacter::changed, this, [this] {
        m_checker.setCharacter(m_character.character(), m_character.build());
        m_finder.setCharacter(m_character.character());
        m_results.judge().setCharacter(m_character.character(), m_character.build());
    });
    // what Path of Building says of the build, which upgrade searches are
    // weighed by
    connect(&m_character, &ActiveCharacter::pobStatsChanged, this,
            [this] { m_finder.setBuild(m_character.build(), m_character.pobStats()); });
    connect(&m_settings, &SettingsStore::changed, this, &App::followSettings);

    const Settings &settings = m_settings.settings();
    // checks go first
    m_weigher.setBusy([this] { return m_checker.isChecking(); });
    m_weigher.setSettings(settings.pob);
    m_character.setId(settings.character);
    lookForPob();
    m_checker.setPob(&m_pob);
    m_checker.setFinder(&m_finder);
    m_finder.setPob(&m_pob);
    m_upgrades.finder().setPob(&m_pob);
    m_checker.setSettings(settings);
    m_checker.setMapsOffered(m_mapsOffered);
    m_results.judge().setPob(&m_pob);
    m_results.judge().setSettings(settings.pob);
    m_resultsPanel.setSettings(settings.overlay);
    connect(&m_checker, &ItemChecker::itemRead, &m_history, &ItemHistory::add);
    connect(&m_checker, &ItemChecker::resultsAsked, &m_resultsPanel, &ResultsPanel::show);
    connect(&m_upgrades, &RecommendedUpgrades::resultsAsked, &m_resultsPanel,
            [this] { m_resultsPanel.show(); });
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
        followRates();
    });
    // what a search finds is what the results have, and they ask the site
    // only once you want to see them
    connect(&m_finder, &TradeFinder::finished, this, [this] {
        if (const std::optional<TradeFinder::Found> &found = m_finder.found())
            m_results.setSearch(*found);
    });
    // the site takes bigger searches signed in, so what it limited them to
    // before isn't kept
    connect(&m_tradeSession, &TradeSession::idChanged, this, [this] {
        m_finder.setSignedIn(m_tradeSession.isSaved());
        m_upgrades.finder().setSignedIn(m_tradeSession.isSaved());
    });
    followTrade();
    m_updates.follow(settings.updates);

    connect(&m_hotkeys, &Hotkeys::pressed, this, [this](Hotkeys::Hotkey hotkey) {
        if (hotkey == Hotkeys::Hotkey::Check)
            m_checker.check();
        else if (hotkey == Hotkeys::Hotkey::Results && m_tradeOffered &&
                 !m_settings.settings().overlay.paused)
            m_resultsPanel.toggle();
    });
    connect(&m_hotkeys, &Hotkeys::keyChanged, this, &App::followKeys);
    m_hotkeys.add(settings.overlay, m_tradeOffered);
    // which have the keys they had last time, when they've been changed on
    // the desktop since
    followKeys();
    m_overlayOptions.startByDefault();
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

void App::updatePob()
{
    lookForPob();
    m_buildImporter.refreshBuilds();
}

void App::followSettings()
{
    const Settings &settings = m_settings.settings();
    m_checker.setSettings(settings);
    m_results.judge().setSettings(settings.pob);
    m_weigher.setSettings(settings.pob);
    m_resultsPanel.setSettings(settings.overlay);
    if (settings.character != m_character.id())
        m_character.setId(settings.character);
    if (settings.pob.location != m_pobLocation)
        lookForPob();
    followTrade();
    m_updates.follow(settings.updates);
}

void App::followKeys()
{
    // without the desktop's shortcuts, they have none
    if (!m_hotkeys.isAvailable())
        return;
    Settings settings = m_settings.settings();
    settings.overlay.hotkey = m_hotkeys.key(Hotkeys::Hotkey::Check);
    // which a release doesn't have, and keeps the key it was given
    if (m_tradeOffered)
        settings.overlay.resultsHotkey = m_hotkeys.key(Hotkeys::Hotkey::Results);
    m_settings.set(settings);
}

void App::followTrade()
{
    // A release has no trade whatever the file says, which is left as it is
    // for a beta to find.
    Settings settings = m_settings.settings();
    if (!m_tradeOffered)
        settings.trade.enabled = false;
    m_trade.setSettings(settings.trade);
    m_finder.setSettings(settings);
    m_checker.popup().setTradeAvailable(m_trade.unavailable().isEmpty());
    m_checker.popup().setTradeNote(m_tradeOffered ? m_trade.unavailable() : QString());
    m_statPicker.setTradeOffered(m_tradeOffered);
    m_upgrades.setTradeAvailable(m_trade.unavailable().isEmpty());
    m_resultsPanel.setUnavailable(m_trade.unavailable());
    m_statPicker.setSiteAllowed(m_trade.unavailable().isEmpty());
    if (m_trade.unavailable().isEmpty()) {
        // which does nothing while what there is is fresh
        m_tradeData.refresh();
        // and this asks the keyring once
        m_tradeSession.load();
    }
    followRates();
}

void App::followRates()
{
    // which carries on as it was when nothing about it changed
    if (m_trade.unavailable().isEmpty() && m_settings.settings().trade.autoRate)
        m_ratesRefresher.follow(m_trade.league().tradeId(), m_settings.settings().trade.rateMinutes);
    else
        m_ratesRefresher.stop();
}

} // namespace pou
