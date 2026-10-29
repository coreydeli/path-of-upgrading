// SPDX-License-Identifier: MIT
#pragma once

#include "platform/notifier.h"

namespace pou {

// Through KNotification, which Plasma shows as a popup.
class KdeNotifier : public Notifier
{
public:
    void notify(Event event, const QString &title, const QString &text) override;
};

} // namespace pou
