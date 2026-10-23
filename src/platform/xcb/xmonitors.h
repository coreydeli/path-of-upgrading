// SPDX-License-Identifier: MIT
#pragma once

#include "platform/screenmap.h"
#include "platform/xcb/xconnection.h"

namespace pou {

// The monitors the X server has, by RandR's names for them, which Xwayland
// takes from the compositor's. None when the server is too old to have
// RandR's monitors.
QList<NamedScreen> xMonitors(XConnection &x);

} // namespace pou
