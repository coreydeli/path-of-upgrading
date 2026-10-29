// SPDX-License-Identifier: MIT
#include "app/desktop.h"

#include "app/commandline.h"
#include "app/hotkeys.h"
#include "app/overlayoptions.h"
#include "platform/notifier.h"
#include "platform/showwindow.h"
#include "platform/tray.h"

#include <QWindow>

using namespace Qt::Literals::StringLiterals;

namespace pou {

Desktop::Desktop(Tray *tray, Notifier *notifier, OverlayOptions *options, Hotkeys *hotkeys, QObject *parent)
    : QObject(parent)
    , m_tray(tray)
    , m_notifier(notifier)
    , m_options(options)
    , m_hotkeys(hotkeys)
{
    connect(tray, &Tray::showRequested, this, &Desktop::showWindow);
    // The menu and the settings window both pause them
    connect(tray, &Tray::pauseRequested, options, &OverlayOptions::setPaused);
    connect(options, &OverlayOptions::changed, this, [this] { m_tray->setPaused(m_options->isPaused()); });
    tray->setPaused(options->isPaused());
}

void Desktop::setWindow(QWindow *window)
{
    m_window = window;
    m_tray->setWindow(window);
}

void Desktop::showWindow()
{
    if (m_window)
        showInFront(m_window);
}

void Desktop::startedAgain(const QStringList &arguments)
{
    // No arguments is how the desktop asks for the window, over D-Bus
    CommandLine commandLine;
    if (!arguments.isEmpty() && commandLine.parse(arguments) && commandLine.background())
        return;
    showWindow();
}

void Desktop::reportProblems()
{
    if (m_hotkeys->isAvailable())
        return;
    m_notifier->notify(Notifier::Event::HotkeysUnavailable, u"The Hotkeys Aren't Working"_s,
                       m_hotkeys->errorString() + u" The settings are in the Path of Upgrading window."_s);
}

} // namespace pou
