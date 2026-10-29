// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <memory>

class QWindow;

namespace pou {

// The program's icon in the system tray, where it waits with its window
// closed. Clicking the icon shows the window, or hides it when it's showing.
// The icon's menu has what you'd do without the window, and Quit.
class Tray : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // The window the icon shows and hides. It has to outlive the tray.
    virtual void setWindow(QWindow *window) = 0;
    // Whether the menu says the hotkeys are paused.
    virtual void setPaused(bool paused) = 0;

Q_SIGNALS:
    // "Show Path of Upgrading" in the menu.
    void showRequested();
    // "Pause the Hotkeys" in the menu, with what it was then changed to.
    void pauseRequested(bool paused);
};

// The one for this desktop.
std::unique_ptr<Tray> createTray();

} // namespace pou
