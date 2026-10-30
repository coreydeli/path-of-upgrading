// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "platform/gameinput.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <optional>

namespace pou {

// Reads the items you check: the one under the mouse when you press the
// hotkey, and gear you copy in game yourself.
class ItemReader : public QObject
{
    Q_OBJECT

public:
    explicit ItemReader(GameInput *input, QObject *parent = nullptr);

    // The hotkey as you'd press it, "F2", for what to do when nothing's read.
    void setHotkey(const QString &key);
    // Copies with Ctrl+Alt+C, which says which mods are prefixes and suffixes.
    void setAdvancedCopy(bool advanced);
    // Also reads the gear you copy yourself.
    void setWatchClipboard(bool watch);
    // Reads nothing at all.
    void setPaused(bool paused);

    // Has the game copy the item under the mouse, which ends in itemRead() or
    // failed(). Does nothing while it can't read. When the popup was in the way
    // and is going, the game is told to nudge the mouse for the item to be
    // under it again.
    void read(bool nudge = false);
    // Whether read() does anything, which it doesn't while paused or reading
    // one already.
    bool canRead() const { return !m_paused && !m_reading; }
    bool isReading() const { return m_reading; }

Q_SIGNALS:
    // The game is taking its time to copy.
    void slow();
    // Whatever the hotkey read, gear or not, and the gear you copied.
    void itemRead(const Item &item, const std::optional<ScreenPoint> &mouse);
    // Why the hotkey read nothing, in words for you.
    void failed(const QString &why, const std::optional<ScreenPoint> &mouse);

private:
    void copied(const QString &text, const std::optional<ScreenPoint> &mouse);
    void notCopied(GameInput::Failure why, const QString &detail, const std::optional<ScreenPoint> &mouse);
    void copiedByHand(const QString &text);
    void finish();
    QString hotkey() const;

    GameInput *m_input;
    QTimer m_slow;
    QString m_hotkey;
    bool m_advanced = false;
    bool m_watchClipboard = true;
    bool m_paused = false;
    bool m_reading = false;
};

} // namespace pou
