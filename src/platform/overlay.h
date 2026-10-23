// SPDX-License-Identifier: MIT
#pragma once

#include <QPoint>

class QScreen;
class QWindow;

namespace pou {

// Windows that stay over the game, fullscreen or not, and never take the
// keyboard from it. On Wayland they're layer shell surfaces in the
// compositor's overlay layer, which is above fullscreen windows, so this
// needs a compositor with layer shell, like KWin.

// Before the window is first shown.
void makeOverlay(QWindow *window);
// Puts the window's top left at that point on that screen, counted from the
// screen's own top left.
void moveOverlay(QWindow *window, QScreen *screen, const QPoint &position);

} // namespace pou
