// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <functional>
#include <memory>

namespace pou {

class Tray;

// Desktop notifications, for the few things worth breaking in on you for.
class Notifier
{
public:
    // The kinds of notification. The desktop lists each in its settings, so
    // you can turn one off.
    enum class Event {
        HotkeysUnavailable, // the desktop wouldn't take the hotkeys
        UpdateAvailable,    // a newer release is out
    };

    // A button on the notification, for desktops that can show one.
    struct Action
    {
        QString text;
        std::function<void()> run;
    };

    virtual ~Notifier() = default;

    virtual void notify(Event event, const QString &title, const QString &text,
                        const Action &action = {}) = 0;
};

// What the desktop knows the event by, which data/path-of-upgrading.notifyrc
// has to list.
QString eventId(Notifier::Event event);

// The one for this desktop, which on Windows shows them from the tray icon.
std::unique_ptr<Notifier> createNotifier(Tray *tray);

} // namespace pou
