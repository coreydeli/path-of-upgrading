// SPDX-License-Identifier: MIT
#include "platform/win/wingamewatcher.h"

#include "platform/win/winwindows.h"

#include <windows.h>

namespace pou {
namespace {

// how often to look, in milliseconds
constexpr int lookTime = 2000;

BOOL CALLBACK checkWindow(HWND window, LPARAM found)
{
    if (!IsWindowVisible(window) || !isGameWindow(describeWindow(window)))
        return TRUE;
    *reinterpret_cast<bool *>(found) = true;
    // no need to look further
    return FALSE;
}

} // namespace

WinGameWatcher::WinGameWatcher(QObject *parent)
    : GameWatcher(parent)
{
    m_timer.setInterval(lookTime);
    connect(&m_timer, &QTimer::timeout, this, &WinGameWatcher::look);
    look();
    m_timer.start();
}

void WinGameWatcher::look()
{
    bool running = false;
    EnumWindows(checkWindow, reinterpret_cast<LPARAM>(&running));
    if (running == m_running)
        return;
    m_running = running;
    Q_EMIT runningChanged(running);
}

} // namespace pou
