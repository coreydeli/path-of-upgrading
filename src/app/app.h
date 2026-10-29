// SPDX-License-Identifier: MIT
#pragma once

#include "app/activecharacter.h"
#include "app/buildimporter.h"
#include "app/characterlist.h"
#include "app/checkeditems.h"
#include "app/gearlist.h"
#include "app/gearscoring.h"
#include "app/hotkeys.h"
#include "app/ignoredstats.h"
#include "app/itemchecker.h"
#include "app/itemhistory.h"
#include "app/overlayoptions.h"
#include "app/poboptions.h"
#include "app/settingsstore.h"
#include "app/statpicker.h"
#include "app/statweights.h"
#include "app/tradefinder.h"
#include "app/tradeoptions.h"
#include "core/characterstore.h"
#include "core/pobinstall.h"
#include "services/pobengine.h"
#include "services/ratesstore.h"
#include "services/tradeclient.h"
#include "services/tradedatastore.h"
#include "services/tradesearcher.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <memory>

class QNetworkAccessManager;
class QQmlEngine;

namespace pou {

class GameInput;
class GlobalShortcuts;

// What the app runs while it's open: the hotkeys, the item checker with its
// popup and the items you've checked, your characters, Path of Building and
// the trade site, all following your settings. The windows show what it has.
class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("There's the one, made when the program starts.")

    // The main window's pages.
    Q_PROPERTY(pou::CharacterList *characterList READ characterList CONSTANT)
    Q_PROPERTY(pou::BuildImporter *buildImporter READ buildImporter CONSTANT)
    Q_PROPERTY(pou::GearList *gearList READ gearList CONSTANT)
    Q_PROPERTY(pou::GearScoring *gearScoring READ gearScoring CONSTANT)
    Q_PROPERTY(pou::StatWeights *statWeights READ statWeights CONSTANT)
    Q_PROPERTY(pou::StatPicker *statPicker READ statPicker CONSTANT)
    Q_PROPERTY(pou::IgnoredStats *ignoredStats READ ignoredStats CONSTANT)
    Q_PROPERTY(pou::CheckedItems *checkedItems READ checkedItems CONSTANT)
    Q_PROPERTY(pou::TradeOptions *tradeOptions READ tradeOptions CONSTANT)
    Q_PROPERTY(pou::OverlayOptions *overlayOptions READ overlayOptions CONSTANT)
    Q_PROPERTY(pou::PobOptions *pobOptions READ pobOptions CONSTANT)

public:
    // Where it keeps what it has between runs.
    struct Paths
    {
        QString settings; // the file
        QString data;     // the folder for your characters and the items you've checked
        QString cache;    // and for what the trade site and poe.ninja said
    };
    // The app's own, in the desktop's usual places.
    static Paths defaultPaths();

    // The engine has to outlive it. Path of Building is looked for in the
    // places, see findPob().
    App(QQmlEngine *qml, std::unique_ptr<GameInput> input, std::unique_ptr<GlobalShortcuts> shortcuts,
        std::unique_ptr<QNetworkAccessManager> network, const PobPlaces &places, const Paths &paths,
        QObject *parent = nullptr);
    ~App() override;

    // Looks for Path of Building again, as once you've installed it.
    Q_INVOKABLE void lookForPob();

    SettingsStore &settings() { return m_settings; }
    CharacterStore &characters() { return m_characters; }
    ActiveCharacter &character() { return m_character; }
    PobEngine &pob() { return m_pob; }
    ItemChecker &checker() { return m_checker; }
    // Their keys are changed here rather than in the settings, which have the
    // keys the desktop gave them.
    Hotkeys &hotkeys() { return m_hotkeys; }
    ItemHistory &history() { return m_history; }
    // Neither site is asked anything while there are no trade searches.
    TradeDataStore &tradeData() { return m_tradeData; }
    RatesStore &rates() { return m_rates; }
    TradeSearcher &trade() { return m_trade; }
    // For the character you play.
    TradeFinder &finder() { return m_finder; }

    CharacterList *characterList() { return &m_characterList; }
    BuildImporter *buildImporter() { return &m_buildImporter; }
    GearList *gearList() { return &m_gearList; }
    GearScoring *gearScoring() { return &m_gearScoring; }
    StatWeights *statWeights() { return &m_statWeights; }
    StatPicker *statPicker() { return &m_statPicker; }
    IgnoredStats *ignoredStats() { return &m_ignoredStats; }
    CheckedItems *checkedItems() { return &m_checkedItems; }
    TradeOptions *tradeOptions() { return &m_tradeOptions; }
    OverlayOptions *overlayOptions() { return &m_overlayOptions; }
    PobOptions *pobOptions() { return &m_pobOptions; }

private:
    void followSettings();
    void followKeys();
    void followTrade();

    PobPlaces m_places;
    SettingsStore m_settings;
    CharacterStore m_characters;
    PobEngine m_pob;
    ActiveCharacter m_character;
    std::unique_ptr<GameInput> m_input;
    ItemChecker m_checker;
    std::unique_ptr<GlobalShortcuts> m_shortcuts;
    Hotkeys m_hotkeys;
    ItemHistory m_history;
    QString m_pobLocation; // where it was last looked for
    std::unique_ptr<QNetworkAccessManager> m_network;
    TradeClient m_tradeClient;
    TradeDataStore m_tradeData;
    RatesStore m_rates;
    TradeSearcher m_trade;
    TradeFinder m_finder;
    CharacterList m_characterList;
    BuildImporter m_buildImporter;
    GearList m_gearList;
    GearScoring m_gearScoring;
    StatWeights m_statWeights;
    StatPicker m_statPicker;
    IgnoredStats m_ignoredStats;
    CheckedItems m_checkedItems;
    TradeOptions m_tradeOptions;
    OverlayOptions m_overlayOptions;
    PobOptions m_pobOptions;
};

} // namespace pou
