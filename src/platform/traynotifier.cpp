// SPDX-License-Identifier: MIT
#include "platform/traynotifier.h"

#include "platform/tray.h"

namespace pou {

TrayNotifier::TrayNotifier(Tray *tray)
    : m_tray(tray)
{}

void TrayNotifier::notify(Event, const QString &title, const QString &text)
{
    if (m_tray)
        m_tray->showMessage(title, text);
}

} // namespace pou
