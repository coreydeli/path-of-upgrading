// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gamewindow.h"
#include "platform/xcb/xconnection.h"

#include <QPoint>

namespace pou {

// The X11 window in front. When that's a Wayland window, KWin names a window
// of its own here, which has no class or process, or none at all.
xcb_window_t activeWindow(XConnection &x);

// What a window says about itself, and the name of its process when we can
// see it in /proc.
WindowInfo describeWindow(XConnection &x, xcb_window_t window);

// Where the mouse is on the X screen, and the program's window it's over.
// Xwayland only follows the mouse over its own windows, so over a Wayland
// one there's no window, and the position is where the mouse left.
struct XPointer
{
    QPoint position;
    xcb_window_t window = XCB_WINDOW_NONE;
};

XPointer queryPointer(XConnection &x);

// Asks the window manager to bring a window to the front, the way a taskbar
// does when it's clicked.
void activate(XConnection &x, xcb_window_t window);

} // namespace pou
