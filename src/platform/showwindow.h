// SPDX-License-Identifier: MIT
#pragma once

class QWindow;

namespace pou {

// Shows the window in front of the others, whether it was hidden, minimized
// or behind them. When the program was started again, the desktop's token
// for that is used, so the desktop lets the window have the focus.
void showInFront(QWindow *window);

} // namespace pou
