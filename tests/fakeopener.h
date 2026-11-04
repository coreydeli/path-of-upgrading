// SPDX-License-Identifier: MIT
#pragma once

#include "platform/opener.h"

#include <QList>
#include <QStringList>

namespace pou {

// Keeps what it's asked to open and copy, and does neither.
class FakeOpener : public Opener
{
public:
    void open(const QUrl &url) override { opened << url; }
    void copy(const QString &text) override { copied << text; }

    QList<QUrl> opened;
    QStringList copied;
};

} // namespace pou
