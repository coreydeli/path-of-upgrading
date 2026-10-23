// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "platform/gameinput.h"

#include <QObject>
#include <QString>
#include <optional>

namespace pou {

// Reads the items you check, the one under the mouse when you press the
// hotkey.
class ItemReader : public QObject
{
    Q_OBJECT

public:
    explicit ItemReader(GameInput *input, QObject *parent = nullptr);

    // The hotkey as you'd press it, "F2", for what to do when nothing's read.
    void setHotkey(const QString &key);
    // Copies with Ctrl+Alt+C, which says which mods are prefixes and suffixes.
    void setAdvancedCopy(bool advanced);
    // Reads nothing at all.
    void setPaused(bool paused);

    // Has the game copy the item under the mouse, which ends in itemRead() or
    // failed(). Does nothing while paused or reading one already.
    void read();
    bool isReading() const { return m_reading; }

Q_SIGNALS:
    // Whatever the hotkey read, gear or not.
    void itemRead(const Item &item, const std::optional<ScreenPoint> &mouse);
    // Why the hotkey read nothing, in words for you.
    void failed(const QString &why, const std::optional<ScreenPoint> &mouse);

private:
    void copied(const QString &text, const std::optional<ScreenPoint> &mouse);
    void notCopied(GameInput::Failure why, const QString &detail, const std::optional<ScreenPoint> &mouse);
    void finish();
    QString hotkey() const;

    GameInput *m_input;
    QString m_hotkey;
    bool m_advanced = false;
    bool m_paused = false;
    bool m_reading = false;
};

} // namespace pou
