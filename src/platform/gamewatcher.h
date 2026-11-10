// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <memory>

namespace pou {

// Tells when the game is running, so the hotkeys can be taken only then and
// left to other programs the rest of the time.
class GameWatcher : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Whether the game has a window open.
    virtual bool isRunning() const = 0;

Q_SIGNALS:
    void runningChanged(bool running);
};

// The one for this desktop, or none when it can't see the game's windows, as
// on a Wayland desktop without Xwayland. The hotkeys then stay on all the time.
std::unique_ptr<GameWatcher> createGameWatcher();

} // namespace pou
