// SPDX-License-Identifier: MIT
#include "platform/win/winglobalshortcuts.h"

#include "platform/win/winkeys.h"

#include <QCoreApplication>
#include <windows.h>

namespace pou {

WinGlobalShortcuts::WinGlobalShortcuts(QObject *parent)
    : GlobalShortcuts(parent)
{
    // they come to this thread, with no window
    QCoreApplication::instance()->installNativeEventFilter(this);
}

WinGlobalShortcuts::~WinGlobalShortcuts()
{
    for (const Shortcut &shortcut : std::as_const(m_shortcuts)) {
        if (!shortcut.key.isEmpty())
            UnregisterHotKey(nullptr, shortcut.number);
    }
}

void WinGlobalShortcuts::add(const QString &id, const QString &, const QKeySequence &key)
{
    Q_ASSERT(!m_shortcuts.contains(id));
    Shortcut shortcut = {.number = ++m_lastNumber};
    if (take(shortcut.number, key))
        shortcut.key = key;
    m_shortcuts.insert(id, shortcut);
}

QKeySequence WinGlobalShortcuts::key(const QString &id) const
{
    return m_shortcuts.value(id).key;
}

bool WinGlobalShortcuts::setKey(const QString &id, const QKeySequence &key)
{
    const auto shortcut = m_shortcuts.find(id);
    if (shortcut == m_shortcuts.end())
        return false;
    if (key == shortcut->key)
        return true;
    const QKeySequence old = shortcut->key;
    if (!old.isEmpty())
        UnregisterHotKey(nullptr, shortcut->number);
    if (!take(shortcut->number, key)) {
        // which it had a moment ago
        take(shortcut->number, old);
        return false;
    }
    shortcut->key = key;
    return true;
}

bool WinGlobalShortcuts::take(int number, const QKeySequence &key)
{
    if (key.isEmpty())
        return true;
    // one key, as Windows has no sequences of them
    if (key.count() != 1)
        return false;
    const std::optional<WinHotKey> hotKey = winHotKey(key[0]);
    return hotKey && RegisterHotKey(nullptr, number, hotKey->modifiers, hotKey->key);
}

bool WinGlobalShortcuts::nativeEventFilter(const QByteArray &, void *message, qintptr *)
{
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message != WM_HOTKEY || msg->hwnd)
        return false;
    for (auto shortcut = m_shortcuts.cbegin(); shortcut != m_shortcuts.cend(); ++shortcut) {
        if (shortcut->number == int(msg->wParam)) {
            Q_EMIT activated(shortcut.key());
            return true;
        }
    }
    return false;
}

} // namespace pou
