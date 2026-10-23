// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gamewindow.h"
#include "platform/xcb/xconnection.h"

namespace pou {

// The X11 window in front. When that's a Wayland window, KWin names a window
// of its own here, which has no class or process, or none at all.
xcb_window_t activeWindow(XConnection &x);

// What a window says about itself, and the name of its process when we can
// see it in /proc.
WindowInfo describeWindow(XConnection &x, xcb_window_t window);

} // namespace pou
