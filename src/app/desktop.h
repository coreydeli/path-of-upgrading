// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

class QWindow;

namespace pou {

class Hotkeys;
class Notifier;
class OverlayOptions;
class Tray;

// What the program does on the desktop outside its window: the tray icon,
// which shows the window and pauses the hotkeys, and the notification for
// when they can't be set up.
class Desktop : public QObject
{
    Q_OBJECT

public:
    // They have to outlive it.
    Desktop(Tray *tray, Notifier *notifier, OverlayOptions *options, Hotkeys *hotkeys,
            QObject *parent = nullptr);

    // The window the tray icon shows and hides.
    void setWindow(QWindow *window);
    // Shows it in front, as the tray's menu does.
    void showWindow();
    // The program was started again, with these arguments. That shows the
    // window, unless it was started in the background, as it is at login.
    void startedAgain(const QStringList &arguments);

    // Says in a notification what's wrong once the program is up, if
    // anything is.
    void reportProblems();

private:
    Tray *m_tray;
    Notifier *m_notifier;
    OverlayOptions *m_options;
    Hotkeys *m_hotkeys;
    QPointer<QWindow> m_window;
};

} // namespace pou
