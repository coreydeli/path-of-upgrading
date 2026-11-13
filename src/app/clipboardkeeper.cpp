// SPDX-License-Identifier: MIT
#include "app/clipboardkeeper.h"

#include "platform/systemclipboard.h"

#include <QMimeData>

using namespace std::chrono_literals;

namespace pou {

ClipboardKeeper::ClipboardKeeper(QObject *parent)
    : QObject(parent)
{
    m_wait.setSingleShot(true);
    m_wait.setInterval(1s);
    connect(&m_wait, &QTimer::timeout, this, &ClipboardKeeper::forget);
    // the copy that putting it back makes comes in soon after, if at all
    m_echo.setSingleShot(true);
    m_echo.setInterval(2s);
    connect(&m_echo, &QTimer::timeout, this, [this] { m_putBack.reset(); });
}

ClipboardKeeper::~ClipboardKeeper() = default;

void ClipboardKeeper::setClipboard(SystemClipboard *clipboard)
{
    if (m_clipboard)
        m_clipboard->disconnect(this);
    forget();
    m_clipboard = clipboard;
    if (clipboard)
        connect(clipboard, &SystemClipboard::changed, this, &ClipboardKeeper::look);
}

void ClipboardKeeper::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
        forget();
}

void ClipboardKeeper::setWaitTime(std::chrono::milliseconds wait)
{
    m_wait.setInterval(wait);
}

void ClipboardKeeper::keep()
{
    forget();
    if (!m_enabled || !m_clipboard)
        return;
    // with nothing copied, there's nothing to put back
    m_kept = m_clipboard->contents();
    if (!m_kept)
        return;
    m_keptText = m_kept->text();
    m_step = Step::Kept;
}

void ClipboardKeeper::putBack(const QString &item)
{
    if (m_step != Step::Kept)
        return;
    m_item = item;
    m_step = Step::Waiting;
    m_wait.start();
    look();
}

void ClipboardKeeper::forget()
{
    m_step = Step::Idle;
    m_wait.stop();
    m_kept.reset();
    m_keptText.clear();
    m_item.clear();
}

bool ClipboardKeeper::isPutBack(const QString &text)
{
    if (!m_putBack || *m_putBack != text)
        return false;
    m_putBack.reset();
    m_echo.stop();
    return true;
}

// On Wayland the game's copy reaches the clipboard through KWin, a moment
// after the X one has come in.
void ClipboardKeeper::look()
{
    if (m_step != Step::Waiting)
        return;
    const QString now = m_clipboard->text();
    if (now == m_item) {
        // once the clipboard has finished telling everyone of the item
        QMetaObject::invokeMethod(this, &ClipboardKeeper::restore, Qt::QueuedConnection);
    } else if (now != m_keptText) {
        // you copied something since, which stays
        forget();
    }
}

void ClipboardKeeper::restore()
{
    if (m_step != Step::Waiting || m_clipboard->text() != m_item)
        return;
    m_putBack = m_keptText;
    m_echo.start();
    // which forget() would delete
    std::unique_ptr<QMimeData> kept = std::move(m_kept);
    forget();
    m_clipboard->setContents(std::move(kept));
}

} // namespace pou
