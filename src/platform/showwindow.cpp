// SPDX-License-Identifier: MIT
#include "platform/showwindow.h"

#include <QWindow>

#ifndef Q_OS_WIN
#include <KWindowSystem>
#endif

namespace pou {

void showInFront(QWindow *window)
{
    // show() would un-maximize it
    window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
    window->setVisible(true);

#ifdef Q_OS_WIN
    // which the copy that was started again lets us do
    window->raise();
    window->requestActivate();
#else
    // KWindowSystem only knows X11 and Wayland
    if (KWindowSystem::platform() == KWindowSystem::Platform::Unknown) {
        window->requestActivate();
        return;
    }
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
#endif
}

} // namespace pou
