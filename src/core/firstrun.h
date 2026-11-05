// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

namespace pou {

// Whether the setup guide shows when the program starts. It does for someone
// who hasn't finished or skipped it, and who hasn't used the program yet: no
// character is saved, and no setting is anything but what a first start
// leaves. Those are the ones the program writes by itself: the two hotkeys the
// desktop gave it, and whether it starts with the desktop.
bool setupWanted(const Settings &settings, bool charactersSaved, bool beta = isBetaBuild());

} // namespace pou
