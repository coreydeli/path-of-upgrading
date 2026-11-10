// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gamewatcher.h"

// The game starting and closing, for tests.
class FakeGameWatcher : public pou::GameWatcher
{
public:
    using pou::GameWatcher::GameWatcher;

    bool isRunning() const override { return running; }

    void setRunning(bool now)
    {
        running = now;
        Q_EMIT runningChanged(now);
    }

    bool running = false;
};
