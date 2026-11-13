// SPDX-License-Identifier: MIT
#include "platform/kde/kdenotifier.h"

#include <KNotification>

namespace pou {

void KdeNotifier::notify(Event event, const QString &title, const QString &text, const Action &action)
{
    // The events are in path-of-upgrading.notifyrc, which it finds by the
    // application's name. The notification deletes itself when it closes.
    auto *notification = new KNotification(eventId(event));
    notification->setTitle(title);
    notification->setText(text);
    if (action.run) {
        KNotificationAction *button = notification->addAction(action.text);
        QObject::connect(button, &KNotificationAction::activated, notification, action.run);
    }
    notification->sendEvent();
}

} // namespace pou
