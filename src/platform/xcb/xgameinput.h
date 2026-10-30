// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gameinput.h"
#include "platform/xcb/xconnection.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

namespace pou {

class XKeySender;
struct XPointer;
class XSelectionWatcher;

// The game as the X11 window it is under Proton, on Xwayland when the
// desktop is Wayland's.
class XGameInput : public GameInput
{
    Q_OBJECT

public:
    explicit XGameInput(QObject *parent = nullptr);

    bool isAvailable() const override;
    QString errorString() const override;
    void copyItem(const CopyOptions &options) override;
    std::optional<ScreenPoint> mouse() const override;

private:
    enum class Step { Idle, PopupGoing, BringingForward, Nudging, Pressing, Waiting };

    std::optional<ScreenPoint> mouseAt(const XPointer &pointer) const;
    void connectToServer();
    void reconnect();
    void poll();
    void findGame();
    void checkPopupGone();
    void checkInFront();
    void press();
    void nudged();
    void keysSent();
    void copied(const QString &text);
    void fail(Failure why, const QString &detail = {});

    std::unique_ptr<XConnection> m_x; // for the windows and the mouse
    QPointer<XSelectionWatcher> m_watcher;
    QPointer<XKeySender> m_keys;
    QTimer m_reconnect;
    QTimer m_poll; // while the popup goes, and the game comes to the front
    QTimer m_timeout;
    QElapsedTimer m_since;

    Step m_step = Step::Idle;
    bool m_advanced = false;
    bool m_nudge = false;
    xcb_window_t m_game = XCB_WINDOW_NONE;
    bool m_overGame = false;
    std::optional<ScreenPoint> m_mouse;
};

} // namespace pou
