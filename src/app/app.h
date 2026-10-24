// SPDX-License-Identifier: MIT
#pragma once

#include "app/activecharacter.h"
#include "app/itemchecker.h"
#include "app/settingsstore.h"
#include "core/characterstore.h"
#include "core/pobinstall.h"
#include "services/pobengine.h"

#include <QObject>
#include <QString>
#include <memory>

class QQmlEngine;

namespace pou {

class GameInput;

// What the app runs while it's open: the item checker with its popup, your
// characters and Path of Building, all following your settings. The windows
// show what it has.
class App : public QObject
{
    Q_OBJECT

public:
    // Where it keeps what it has between runs.
    struct Paths
    {
        QString settings; // the file
        QString data;     // the folder for your characters
    };
    // The app's own, in the desktop's usual places.
    static Paths defaultPaths();

    // The engine has to outlive it. Path of Building is looked for in the
    // places, see findPob().
    App(QQmlEngine *qml, std::unique_ptr<GameInput> input, const PobPlaces &places, const Paths &paths,
        QObject *parent = nullptr);
    ~App() override;

    // Looks for Path of Building again, as once you've installed it.
    void lookForPob();

    SettingsStore &settings() { return m_settings; }
    CharacterStore &characters() { return m_characters; }
    ActiveCharacter &character() { return m_character; }
    PobEngine &pob() { return m_pob; }
    ItemChecker &checker() { return m_checker; }

private:
    void followSettings();

    PobPlaces m_places;
    SettingsStore m_settings;
    CharacterStore m_characters;
    PobEngine m_pob;
    ActiveCharacter m_character;
    std::unique_ptr<GameInput> m_input;
    ItemChecker m_checker;
    QString m_pobLocation; // where it was last looked for
};

} // namespace pou
