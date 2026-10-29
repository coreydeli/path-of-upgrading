// SPDX-License-Identifier: MIT
#pragma once

#include "platform/tray.h"

// A tray that only remembers what it was told, so no icon reaches the desktop.
class FakeTray : public pou::Tray
{
public:
    using pou::Tray::Tray;

    void setWindow(QWindow *window) override { this->window = window; }
    void setPaused(bool paused) override { this->paused = paused; }

    // as the menu does
    void chooseShow() { Q_EMIT showRequested(); }
    void choosePause(bool paused)
    {
        this->paused = paused;
        Q_EMIT pauseRequested(paused);
    }

    QWindow *window = nullptr;
    bool paused = false;
};
