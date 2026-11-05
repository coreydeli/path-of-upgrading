// SPDX-License-Identifier: MIT
#pragma once

#include "platform/notifier.h"

#include <QPointer>

namespace pou {

class Tray;

// As a message from the tray icon, where Windows shows them.
class TrayNotifier : public Notifier
{
public:
    explicit TrayNotifier(Tray *tray);

    void notify(Event event, const QString &title, const QString &text) override;

private:
    QPointer<Tray> m_tray;
};

} // namespace pou
