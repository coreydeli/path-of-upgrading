// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gameinput.h"

#include <QElapsedTimer>
#include <QTimer>

namespace pou {

class WinKeySender;

// The game as the window it is on Windows.
class WinGameInput : public GameInput
{
    Q_OBJECT

public:
    explicit WinGameInput(QObject *parent = nullptr);

    bool isAvailable() const override { return true; }
    QString errorString() const override { return {}; }
    void copyItem(const CopyOptions &options) override;
    std::optional<ScreenPoint> mouse() const override;

private:
    enum class Step { Idle, PopupGoing, BringingForward, Nudging, Pressing, Waiting };

    void poll();
    void findGame();
    void checkPopupGone();
    void checkInFront();
    void press();
    void nudged();
    void keysSent();
    void clipboardChanged();
    void fail(Failure why, const QString &detail = {});

    WinKeySender *m_keys;
    QTimer m_poll; // while the popup goes, and the game comes to the front
    QTimer m_timeout;
    QElapsedTimer m_since;

    Step m_step = Step::Idle;
    bool m_advanced = false;
    bool m_nudge = false;
    quintptr m_game = 0; // its window
    bool m_overGame = false;
    std::optional<ScreenPoint> m_mouse;
};

} // namespace pou
