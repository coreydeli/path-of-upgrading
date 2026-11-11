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
        QString action; // the button's text, if it had one
    };

    void notify(Event event, const QString &title, const QString &text, const Action &action = {}) override
    {
        sent.append({event, title, text, action.text});
        actions.append(action.run);
    }

    QList<Sent> sent;
    // what each one's button does
    QList<std::function<void()>> actions;
};
