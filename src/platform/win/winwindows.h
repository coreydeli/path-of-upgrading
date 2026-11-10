// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gamewindow.h"

#include <windows.h>

namespace pou {

// The id of the process a window belongs to.
DWORD processOf(HWND window);

// A window's class and the file name of its program.
WindowInfo describeWindow(HWND window);

} // namespace pou
