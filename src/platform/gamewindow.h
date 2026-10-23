// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

namespace pou {

// What there is to go on when telling the game's window from the others.
struct WindowInfo
{
    // Both parts of WM_CLASS on X11. Wine puts the program's name there, and
    // Proton puts steam_app_ and the game's Steam id.
    QStringList classes;
    // The program's file name, when its process can be seen.
    QString program;
};

bool isGameWindow(const WindowInfo &window);

} // namespace pou
