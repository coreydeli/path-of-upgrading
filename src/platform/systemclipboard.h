// SPDX-License-Identifier: MIT
#pragma once

#include <QDeadlineTimer>
#include <QObject>
#include <chrono>
#include <memory>

class QMimeData;

namespace pou {

// The desktop's clipboard, read and set while the game has the focus and the
// app's windows don't.
class SystemClipboard : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // A copy of all that's on it, or none when it's empty or couldn't be read
    // in time.
    virtual std::unique_ptr<QMimeData> contents() = 0;
    virtual QString text() = 0;
    virtual void setContents(std::unique_ptr<QMimeData> data) = 0;

Q_SIGNALS:
    // When anything is copied, this included.
    void changed();
};

// The one for this desktop.
std::unique_ptr<SystemClipboard> createSystemClipboard();

// Every format of what's copied, read at once, as it's gone once something
// else is copied. Formats with nothing in them are left out. None when there's
// nothing, when it's over the size, or when reading it runs past the deadline,
// as one that hangs would hold up the app.
std::unique_ptr<QMimeData> copyOf(const QMimeData *data, qsizetype mostBytes = 64 * 1024 * 1024,
                                  QDeadlineTimer deadline = QDeadlineTimer(std::chrono::milliseconds(500)));

} // namespace pou
