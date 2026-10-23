// SPDX-License-Identifier: MIT
#include "platform/globalshortcuts.h"

#include "platform/kde/kdeglobalshortcuts.h"

namespace pou {

std::unique_ptr<GlobalShortcuts> createGlobalShortcuts()
{
    return std::make_unique<KdeGlobalShortcuts>();
}

} // namespace pou
