// SPDX-License-Identifier: MIT
#pragma once

#include "app/itemjudge.h"
#include "app/itempopup.h"
#include "app/itemreader.h"

#include <QObject>
#include <optional>

class QQmlEngine;

namespace pou {

class GameInput;
class PobEngine;

// What the hotkey does: reads the item under the mouse, judges it for the
// character you play and shows that in the popup, then shows it again once
// Path of Building has answered. Gear you copy yourself goes the same way.
class ItemChecker : public QObject
{
    Q_OBJECT

public:
    // The engine and the input have to outlive it.
    ItemChecker(QQmlEngine *engine, GameInput *input, QObject *parent = nullptr);

    void setSettings(const Settings &settings);
    // The character you play and its build, none when it has none.
    void setCharacter(const Character &character, const std::optional<PobBuild> &build);
    // Path of Building, used while the settings have it on.
    void setPob(PobEngine *pob);

    // For the hotkey.
    void check();

    ItemPopup &popup() { return m_popup; }

Q_SIGNALS:
    // Each item read, gear or not.
    void itemRead(const Item &item);

private:
    void read(const Item &item, const std::optional<ScreenPoint> &mouse);
    void judged(const ItemCheck &check);
    void judgeWithPob();

    GameInput *m_input;
    ItemReader m_reader;
    ItemJudge m_judge;
    ItemPopup m_popup;
    PobEngine *m_pob = nullptr;
    bool m_pobOn = true;
    std::optional<ScreenPoint> m_mouse; // where the item was read
    bool m_fresh = false;               // and it hasn't been shown yet
};

} // namespace pou
