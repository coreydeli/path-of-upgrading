// SPDX-License-Identifier: MIT
#pragma once

#include "platform/tray.h"

class KStatusNotifierItem;
class QAction;

namespace pou {

// As a status notifier item, which Plasma shows in its system tray.
class KdeTray : public Tray
{
    Q_OBJECT

public:
    explicit KdeTray(QObject *parent = nullptr);

    void setWindow(QWindow *window) override;
    void setPaused(bool paused) override;

private:
    KStatusNotifierItem *m_item;
    QAction *m_pause;
};

} // namespace pou
