// SPDX-License-Identifier: MIT
#pragma once

#include "platform/copykeys.h"

#include <QElapsedTimer>
#include <QObject>

namespace pou {

// Presses keys for the window that has the keyboard, as if they were typed,
// and nudges the mouse, with SendInput.
class WinKeySender : public QObject
{
    Q_OBJECT

public:
    explicit WinKeySender(QObject *parent = nullptr);
    // lets go of the keys it's holding
    ~WinKeySender() override;

    bool isBusy() const { return m_busy; }

    // The game's copy, once the hotkey's keys are up.
    void copy(bool advanced);
    // Moves the mouse a pixel and back, for the game to see what's under it
    // again, as it doesn't while the mouse stays still. See nudged().
    void nudge();

Q_SIGNALS:
    void sent();
    // The mouse is back, and the game has had a moment to show what's under it.
    void nudged();
    void failed(const QString &why);

private:
    void waitForKeys();
    void sendSteps();

    QElapsedTimer m_waiting;
    QList<KeyStep> m_steps;
    bool m_advanced = false;
    bool m_busy = false;
};

} // namespace pou
