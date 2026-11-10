// SPDX-License-Identifier: MIT
#pragma once

#include "platform/gamewatcher.h"

#include <QTimer>

namespace pou {

// Windows doesn't say when programs start, so this looks at the windows every
// couple of seconds, which costs next to nothing.
class WinGameWatcher : public GameWatcher
{
    Q_OBJECT

public:
    explicit WinGameWatcher(QObject *parent = nullptr);

    bool isRunning() const override { return m_running; }

private:
    void look();

    QTimer m_timer;
    bool m_running = false;
};

} // namespace pou
