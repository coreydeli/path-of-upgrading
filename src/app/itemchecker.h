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

// What the hotkey does: reads the item under the mouse, judges it for the
// character you play and shows that in the popup. Gear you copy yourself goes
// the same way.
class ItemChecker : public QObject
{
    Q_OBJECT

public:
    // The engine and the input have to outlive it.
    ItemChecker(QQmlEngine *engine, GameInput *input, QObject *parent = nullptr);

    void setSettings(const Settings &settings);
    void setCharacter(const Character &character);

    // For the hotkey.
    void check();

    ItemPopup &popup() { return m_popup; }

Q_SIGNALS:
    // Each item read, gear or not.
    void itemRead(const Item &item);

private:
    void read(const Item &item, const std::optional<ScreenPoint> &mouse);
    void judged(const ItemCheck &check);

    ItemReader m_reader;
    ItemJudge m_judge;
    ItemPopup m_popup;
    std::optional<ScreenPoint> m_mouse; // where the item was read
};

} // namespace pou
