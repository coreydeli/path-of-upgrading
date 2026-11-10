// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QString>

namespace pou {

class GameWatcher;
class GlobalShortcuts;

// The app's keys that work while the game has the keyboard. The desktop keeps
// them, so on KDE they show in System Settings too. With a game watcher they
// are only taken from other programs while the game is running, so a key like
// 2 can still be typed everywhere else.
class Hotkeys : public QObject
{
    Q_OBJECT

public:
    enum class Hotkey {
        Check,   // checks the item under the mouse
        Results, // shows the trade results for it
    };
    Q_ENUM(Hotkey)

    // The shortcuts and the watcher have to outlive it. Without a watcher the
    // keys are always taken.
    explicit Hotkeys(GlobalShortcuts *shortcuts, GameWatcher *game = nullptr, QObject *parent = nullptr);

    // Adds them with the keys in the settings, which the desktop only takes
    // the first time. After that each has the key it had there. Without
    // results it leaves that one out.
    void add(const OverlaySettings &settings, bool withResults = true);

    // As you'd press it, "F2", or empty when it has none.
    QString key(Hotkey hotkey) const;
    // False when the key can't be read, or another program or the other
    // hotkey has it, and it keeps the one it had. Empty takes its key away.
    bool setKey(Hotkey hotkey, const QString &key);

    // Whether they're only taken while the game is running.
    bool followsGame() const { return m_game != nullptr; }
    bool isAvailable() const;
    QString errorString() const;

Q_SIGNALS:
    void pressed(pou::Hotkeys::Hotkey hotkey);
    // Also when it's changed outside the app, as in System Settings.
    void keyChanged(pou::Hotkeys::Hotkey hotkey);

private:
    GlobalShortcuts *m_shortcuts;
    GameWatcher *m_game;
};

} // namespace pou
