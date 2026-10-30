// SPDX-License-Identifier: MIT
#pragma once

#include "platform/copykeys.h"
#include "platform/xcb/xconnection.h"

#include <QElapsedTimer>
#include <QObject>

namespace pou {

// Presses keys for the window that has the keyboard, as if they were typed,
// and nudges the mouse. That's XTest, which Xwayland hands on to KWin, and KWin
// may ask the user to allow it.
class XKeySender : public QObject
{
    Q_OBJECT

public:
    explicit XKeySender(QObject *parent = nullptr);
    // lets go of the keys it's holding
    ~XKeySender() override;

    bool isValid() const;
    QString errorString() const;
    bool isBusy() const { return m_busy; }

    // The game's copy, once the hotkey's keys are up.
    void copy(bool advanced);
    // Moves the mouse a pixel and back, for the game to see what's under it
    // again, as it doesn't while the mouse stays still. See nudged().
    void nudge();

    // By the codes the keyboard map has for them now.
    CopyKeys copyKeys();

Q_SIGNALS:
    void sent();
    // The mouse is back, and the game has had a moment to show what's under it.
    void nudged();
    void failed(const QString &why);

private:
    void waitForKeys();
    void sendSteps();
    void send(const KeyStep &step);
    void moveMouse(int dx);

    XConnection m_x;
    QString m_errorString;
    QElapsedTimer m_waiting;
    QList<KeyStep> m_steps;
    bool m_advanced = false;
    bool m_busy = false;
};

} // namespace pou
