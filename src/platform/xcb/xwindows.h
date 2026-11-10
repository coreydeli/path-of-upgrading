// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gamewindow.h"
#include "platform/screenmap.h"
#include "platform/xcb/xconnection.h"

#include <QPoint>
#include <optional>

namespace pou {

// The X11 window in front. When that's a Wayland window, KWin names a window
// of its own here, which has no class or process, or none at all.
xcb_window_t activeWindow(XConnection &x);

// What a window says about itself, and the name of its process when we can
// see it in /proc.
WindowInfo describeWindow(XConnection &x, xcb_window_t window);

// Whether any of these windows is the game's.
bool anyGameWindow(XConnection &x, const QList<xcb_window_t> &windows);

// Where the mouse is on the X screen, and the program's window it's over.
// The position is the X server's own, which KWin's Xwayland keeps up with over
// Wayland windows too. The window is only there when the mouse is over one of
// the program's: over a Wayland window, or under a window that covers the
// screen without being a program's, like Wine's desktop, there's none.
struct XPointer
{
    std::optional<QPoint> position; // none when the server wouldn't say
    xcb_window_t window = XCB_WINDOW_NONE;
};

XPointer queryPointer(XConnection &x);

// Where the mouse is on Qt's screens, given the monitors the X server has and
// the screens Qt has. It doesn't matter what window it's over, only that the
// server knows where it is and it's on a monitor.
std::optional<ScreenPoint> pointerOnScreens(const XPointer &pointer, const QList<NamedScreen> &monitors,
                                            const QList<NamedScreen> &screens);

// Asks the window manager to bring a window to the front, the way a taskbar
// does when it's clicked.
void activate(XConnection &x, xcb_window_t window);

} // namespace pou
