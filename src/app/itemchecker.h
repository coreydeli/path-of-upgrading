// SPDX-License-Identifier: MIT
#pragma once

#include "app/itemjudge.h"
#include "app/itempopup.h"
#include "app/itemreader.h"

#include <QObject>
#include <QPointer>
#include <optional>

class QQmlEngine;

namespace pou {

class GameInput;
class PobEngine;

// What the hotkey does: reads the item under the mouse, judges it for the
// character you play and shows that in the popup, then shows it again once
// Path of Building has answered. Gear you copy yourself goes the same way.
// The popup's buttons search the trade site for the item, or say you wear it.
class ItemChecker : public QObject
{
    Q_OBJECT

public:
    // The engine and the input have to outlive it.
    ItemChecker(QQmlEngine *engine, GameInput *input, QObject *parent = nullptr);

    void setSettings(const Settings &settings);
    // The character you play and its build, none when it has none. The item
    // the popup shows is judged again for it.
    void setCharacter(const Character &character, const std::optional<PobBuild> &build);
    // Path of Building, used while the settings have it on.
    void setPob(PobEngine *pob);
    // What the popup's buttons search with, once.
    void setFinder(TradeFinder *finder);

    // For the hotkey.
    void check();
    // Judges an item you have already, as when you pick it from the ones you
    // checked. Nothing says where the mouse is, so the popup goes where it does
    // then.
    void checkItem(const Item &item);

    ItemPopup &popup() { return m_popup; }

Q_SIGNALS:
    // Each item read, gear or not.
    void itemRead(const Item &item);
    // When you say in the popup you wear the item in the slot.
    void wearAsked(const Item &item, const QString &slot);

private:
    void read(const Item &item, const std::optional<ScreenPoint> &mouse);
    void judged(const ItemCheck &check);
    void judgeWithPob();
    void findOnTrade(TradeFinder::Kind kind);
    void found();

    GameInput *m_input;
    ItemReader m_reader;
    ItemJudge m_judge;
    ItemPopup m_popup;
    PobEngine *m_pob = nullptr;
    QPointer<TradeFinder> m_finder; // null once it's gone
    bool m_pobOn = true;
    std::optional<ScreenPoint> m_mouse; // where the item was read
    bool m_fresh = false;               // and it hasn't been shown yet
};

} // namespace pou
