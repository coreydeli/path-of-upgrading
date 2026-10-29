// SPDX-License-Identifier: MIT
#pragma once

#include "platform/notifier.h"

#include <QList>

// A notifier that only remembers what it was asked to say, so nothing is
// shown on the desktop.
class FakeNotifier : public pou::Notifier
{
public:
    struct Sent
    {
        Event event;
        QString title;
        QString text;
    };

    void notify(Event event, const QString &title, const QString &text) override
    {
        sent.append({event, title, text});
    }

    QList<Sent> sent;
};
