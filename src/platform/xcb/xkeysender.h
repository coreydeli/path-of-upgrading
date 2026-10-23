// SPDX-License-Identifier: MIT
#pragma once

#include "platform/copykeys.h"
#include "platform/xcb/xconnection.h"

#include <QElapsedTimer>
#include <QObject>

namespace pou {

// Presses keys for the window that has the keyboard, as if they were typed.
// That's XTest, which Xwayland hands on to KWin, and KWin asks the user once
// whether to allow it.
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

    // By the codes the keyboard map has for them now.
    CopyKeys copyKeys();

Q_SIGNALS:
    void sent();
    void failed(const QString &why);

private:
    void waitForKeys();
    void sendSteps();
    void send(const KeyStep &step);

    XConnection m_x;
    QString m_errorString;
    QElapsedTimer m_waiting;
    QList<KeyStep> m_steps;
    bool m_advanced = false;
    bool m_busy = false;
};

} // namespace pou
