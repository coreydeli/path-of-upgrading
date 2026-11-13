// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

class QSettings;

namespace pou {

// Your settings, in the file QSettings keeps them in.

// Anything missing or unreadable reads as its default, and numbers are held
// to their limits. Trade searches are on or off as a beta or a release starts
// them, until the file says what you chose.
Settings loadSettings(const QSettings &store, bool beta = isBetaBuild());
// Leaves out whether trade searches are on while it's what a beta or a release
// starts with and you haven't chosen, so a later version still starts them its
// own way.
void saveSettings(QSettings &store, const Settings &settings, bool beta = isBetaBuild());

} // namespace pou
