// SPDX-License-Identifier: MIT
#include "platform/gamewatcher.h"

#ifdef Q_OS_WIN
#include "platform/win/wingamewatcher.h"
#else
#include "platform/xcb/xgamewatcher.h"
#endif

namespace pou {

std::unique_ptr<GameWatcher> createGameWatcher()
{
#ifdef Q_OS_WIN
    return std::make_unique<WinGameWatcher>();
#else
    auto watcher = std::make_unique<XGameWatcher>();
    if (!watcher->isValid())
        return nullptr;
    return watcher;
#endif
}

} // namespace pou
