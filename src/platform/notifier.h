// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <memory>

namespace pou {

// Desktop notifications, for the few things worth breaking in on you for.
class Notifier
{
public:
    // The kinds of notification. The desktop lists each in its settings, so
    // you can turn one off.
    enum class Event {
        HotkeysUnavailable, // the desktop wouldn't take the hotkeys
    };

    virtual ~Notifier() = default;

    virtual void notify(Event event, const QString &title, const QString &text) = 0;
};

// What the desktop knows the event by, which data/path-of-upgrading.notifyrc
// has to list.
QString eventId(Notifier::Event event);

// The one for this desktop.
std::unique_ptr<Notifier> createNotifier();

} // namespace pou
