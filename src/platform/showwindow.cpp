// SPDX-License-Identifier: MIT
#include "platform/showwindow.h"

#include <KWindowSystem>
#include <QWindow>

namespace pou {

void showInFront(QWindow *window)
{
    // show() would un-maximize it
    window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
    window->setVisible(true);

    // KWindowSystem only knows X11 and Wayland
    if (KWindowSystem::platform() == KWindowSystem::Platform::Unknown) {
        window->requestActivate();
        return;
    }
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
}

} // namespace pou
