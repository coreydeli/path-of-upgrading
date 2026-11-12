// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gamewatcher.h"
#include "platform/xcb/xconnection.h"

#include <QSocketNotifier>
#include <QTimer>
#include <memory>

namespace pou {

// The game is running while the window manager lists a window of it. Under
// Proton on a Wayland desktop that's an Xwayland window, and the window
// manager says when its list changes, so there's nothing to poll. Looking at
// the windows is also how the hotkey finds the game, so what this counts as
// running is what the hotkey can work with.
class XGameWatcher : public GameWatcher
{
    Q_OBJECT

public:
    explicit XGameWatcher(QObject *parent = nullptr);

    // Whether there's an X server to watch.
    bool isValid() const { return m_x->isValid(); }
    bool isRunning() const override { return m_running; }

private:
    void watch();
    void readEvents();
    void reconnect();
    void look();
    void setRunning(bool running);

    std::unique_ptr<XConnection> m_x;
    bool m_running = false;
    QTimer m_reconnect;
    // after the connection, so it goes first
    QSocketNotifier m_notifier{QSocketNotifier::Read, this};
};

} // namespace pou
