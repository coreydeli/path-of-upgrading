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
        if (shortcut.held)
            UnregisterHotKey(nullptr, shortcut.number);
    }
}

void WinGlobalShortcuts::add(const QString &id, const QString &, const QKeySequence &key)
{
    Q_ASSERT(!m_shortcuts.contains(id));
    Shortcut shortcut = {.number = ++m_lastNumber, .key = key};
    if (m_grabbed) {
        shortcut.held = !key.isEmpty() && take(shortcut.number, key);
        if (!shortcut.held)
            shortcut.key = QKeySequence();
    }
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
    if (!m_grabbed) {
        // it can't be tried without taking it, and it mustn't be another of ours
        for (auto other = m_shortcuts.cbegin(); !key.isEmpty() && other != m_shortcuts.cend(); ++other) {
            if (other.key() != id && other->key == key)
                return false;
        }
        const bool free = key.isEmpty() || take(shortcut->number, key);
        if (free && !key.isEmpty())
            UnregisterHotKey(nullptr, shortcut->number);
        if (free)
            shortcut->key = key;
        return free;
    }
    const QKeySequence old = shortcut->key;
    if (shortcut->held)
        UnregisterHotKey(nullptr, shortcut->number);
    if (!take(shortcut->number, key)) {
        // which it had a moment ago
        shortcut->held = take(shortcut->number, old) && !old.isEmpty();
        return false;
    }
    shortcut->key = key;
    shortcut->held = !key.isEmpty();
    return true;
}

void WinGlobalShortcuts::setGrabbed(bool grabbed)
{
    if (grabbed == m_grabbed)
        return;
    m_grabbed = grabbed;
    for (auto shortcut = m_shortcuts.begin(); shortcut != m_shortcuts.end(); ++shortcut) {
        if (!grabbed) {
            if (shortcut->held)
                UnregisterHotKey(nullptr, shortcut->number);
            shortcut->held = false;
            continue;
        }
        if (shortcut->key.isEmpty())
            continue;
        shortcut->held = take(shortcut->number, shortcut->key);
        if (!shortcut->held) {
            // another program has it now
            shortcut->key = QKeySequence();
            Q_EMIT keyChanged(shortcut.key(), QKeySequence());
        }
    }
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
