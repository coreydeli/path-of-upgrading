// SPDX-License-Identifier: MIT
#include "platform/globalshortcuts.h"

#ifdef Q_OS_WIN
#include "platform/win/winglobalshortcuts.h"
#else
#include "platform/kde/kdeglobalshortcuts.h"
#endif

namespace pou {

std::unique_ptr<GlobalShortcuts> createGlobalShortcuts()
{
#ifdef Q_OS_WIN
    return std::make_unique<WinGlobalShortcuts>();
#else
    return std::make_unique<KdeGlobalShortcuts>();
#endif
}

} // namespace pou
