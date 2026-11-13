// SPDX-License-Identifier: MIT
#pragma once

#include "app/ownstyle.h"

#include <QtGlobal>

// The tests that load QML use the app's own style, as the program does. Its
// Kirigami plugin is built in the build folder.
inline void useStyleInTests()
{
    qputenv("QT_PLUGIN_PATH", POU_STYLE_PLUGINS);
    pou::useOwnStyle();
}
