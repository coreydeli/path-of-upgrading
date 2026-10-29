// SPDX-License-Identifier: MIT
#include "platform/tray.h"

#include "platform/kde/kdetray.h"

namespace pou {

std::unique_ptr<Tray> createTray()
{
    return std::make_unique<KdeTray>();
}

} // namespace pou
