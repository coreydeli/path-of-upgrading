// SPDX-License-Identifier: MIT
#include "platform/gamewindow.h"

#include <algorithm>

namespace pou {
namespace {

// Path of Exile 2 on Steam
constexpr QStringView steamClass = u"steam_app_2694490";

// PathOfExile.exe, or PathOfExileSteam.exe for the one from Steam
bool isGameProgram(QStringView name)
{
    return name.startsWith(u"PathOfExile", Qt::CaseInsensitive);
}

} // namespace

bool isGameWindow(const WindowInfo &window)
{
    return window.classes.contains(steamClass) || std::ranges::any_of(window.classes, isGameProgram) ||
           isGameProgram(window.program);
}

} // namespace pou
