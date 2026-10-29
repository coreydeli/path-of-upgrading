// SPDX-License-Identifier: MIT
#include "platform/kde/kdenotifier.h"

#include <KNotification>

namespace pou {

void KdeNotifier::notify(Event event, const QString &title, const QString &text)
{
    // The events are in path-of-upgrading.notifyrc, which it finds by the
    // application's name. The notification deletes itself when it closes.
    auto *notification = new KNotification(eventId(event));
    notification->setTitle(title);
    notification->setText(text);
    notification->sendEvent();
}

} // namespace pou
