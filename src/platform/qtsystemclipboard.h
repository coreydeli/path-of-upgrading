// SPDX-License-Identifier: MIT
#pragma once

#include "platform/systemclipboard.h"

namespace pou {

// Qt's own clipboard, which on Windows works whichever window has the focus.
class QtSystemClipboard : public SystemClipboard
{
    Q_OBJECT

public:
    explicit QtSystemClipboard(QObject *parent = nullptr);

    std::unique_ptr<QMimeData> contents() override;
    QString text() override;
    void setContents(std::unique_ptr<QMimeData> data) override;
};

} // namespace pou
