// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QString>

namespace pou {

class GlobalShortcuts;

// The app's keys that work while the game has the keyboard. The desktop keeps
// them, so on KDE they show in System Settings too.
class Hotkeys : public QObject
{
    Q_OBJECT

public:
    enum class Hotkey {
        Check,   // checks the item under the mouse
        Results, // shows the trade results for it
    };
    Q_ENUM(Hotkey)

    // The shortcuts have to outlive it.
    explicit Hotkeys(GlobalShortcuts *shortcuts, QObject *parent = nullptr);

    // Adds them with the keys in the settings, which the desktop only takes
    // the first time. After that each has the key it had there.
    void add(const OverlaySettings &settings);

    // As you'd press it, "F2", or empty when it has none.
    QString key(Hotkey hotkey) const;
    // False when the key can't be read, or another program or the other
    // hotkey has it, and it keeps the one it had. Empty takes its key away.
    bool setKey(Hotkey hotkey, const QString &key);

    bool isAvailable() const;
    QString errorString() const;

Q_SIGNALS:
    void pressed(pou::Hotkeys::Hotkey hotkey);
    // Also when it's changed outside the app, as in System Settings.
    void keyChanged(pou::Hotkeys::Hotkey hotkey);

private:
    GlobalShortcuts *m_shortcuts;
};

} // namespace pou
