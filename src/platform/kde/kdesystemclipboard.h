// SPDX-License-Identifier: MIT
#pragma once

#include "platform/systemclipboard.h"

class KSystemClipboard;

namespace pou {

// The clipboard through KDE's KSystemClipboard, which on Wayland reads and
// sets it without the focus, where the compositor allows it (KWin does). On X,
// and where it doesn't, it's Qt's own.
class KdeSystemClipboard : public SystemClipboard
{
    Q_OBJECT

public:
    explicit KdeSystemClipboard(QObject *parent = nullptr);

    std::unique_ptr<QMimeData> contents() override;
    QString text() override;
    void setContents(std::unique_ptr<QMimeData> data) override;

private:
    // KDE's, shared by the whole program, and none while it's closing
    KSystemClipboard *m_clipboard;
};

} // namespace pou
