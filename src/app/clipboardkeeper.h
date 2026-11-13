// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <chrono>
#include <memory>
#include <optional>

class QMimeData;

namespace pou {

class SystemClipboard;

// Puts back what you had copied once the game has copied an item over it for
// the hotkey. Copies you make yourself are left alone.
class ClipboardKeeper : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardKeeper(QObject *parent = nullptr);
    ~ClipboardKeeper() override;

    // Does nothing without one.
    void setClipboard(SystemClipboard *clipboard);
    void setEnabled(bool enabled);
    // How long the clipboard gets to show the item before it's left as it is.
    // Tests wait less.
    void setWaitTime(std::chrono::milliseconds wait);

    // Before the game copies.
    void keep();
    // Once the game has copied the item, which goes once the clipboard has it,
    // unless something else was copied first.
    void putBack(const QString &item);
    // When the game copied nothing.
    void forget();

    // Whether this is what was just put back, which comes in again as a copy.
    // It's only that once.
    bool isPutBack(const QString &text);

private:
    enum class Step { Idle, Kept, Waiting };

    void look();
    void restore();

    SystemClipboard *m_clipboard = nullptr;
    bool m_enabled = true;
    Step m_step = Step::Idle;
    std::unique_ptr<QMimeData> m_kept;
    QString m_keptText;
    QString m_item;
    std::optional<QString> m_putBack;
    QTimer m_wait;
    QTimer m_echo;
};

} // namespace pou
