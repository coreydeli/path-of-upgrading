// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gameinput.h"

// The game for tests, which copies nothing until the test says what it
// copied, and counts what it was asked for.
class FakeGameInput : public pou::GameInput
{
public:
    using pou::GameInput::GameInput;

    bool isAvailable() const override { return true; }
    QString errorString() const override { return {}; }

    void copyItem(bool advanced) override
    {
        ++copies;
        lastAdvanced = advanced;
        // the way it fails when it can't press keys at all
        if (!failRightAway.isEmpty())
            Q_EMIT copyFailed(Failure::Unavailable, failRightAway, std::nullopt);
    }

    std::optional<pou::ScreenPoint> mouse() const override { return mouseAt; }

    int copies = 0;
    bool lastAdvanced = false;
    QString failRightAway;
    std::optional<pou::ScreenPoint> mouseAt;
};
