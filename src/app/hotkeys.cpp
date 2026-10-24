// SPDX-License-Identifier: MIT
#include "app/hotkeys.h"

#include "platform/globalshortcuts.h"

#include <QKeySequence>
#include <optional>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// These are what the desktop knows them by, so they mustn't change.
QString idOf(Hotkeys::Hotkey hotkey)
{
    switch (hotkey) {
    case Hotkeys::Hotkey::Check:
        return u"check-item"_s;
    case Hotkeys::Hotkey::Results:
        return u"trade-results"_s;
    }
    Q_UNREACHABLE_RETURN({});
}

std::optional<Hotkeys::Hotkey> hotkeyOf(const QString &id)
{
    for (const Hotkeys::Hotkey hotkey : {Hotkeys::Hotkey::Check, Hotkeys::Hotkey::Results}) {
        if (idOf(hotkey) == id)
            return hotkey;
    }
    return std::nullopt;
}

QKeySequence sequenceOf(const QString &key)
{
    return QKeySequence::fromString(key, QKeySequence::PortableText);
}

QString textOf(const QKeySequence &sequence)
{
    return sequence.toString(QKeySequence::PortableText);
}

} // namespace

Hotkeys::Hotkeys(GlobalShortcuts *shortcuts, QObject *parent)
    : QObject(parent)
    , m_shortcuts(shortcuts)
{
    connect(shortcuts, &GlobalShortcuts::activated, this, [this](const QString &id) {
        if (const std::optional<Hotkey> hotkey = hotkeyOf(id))
            Q_EMIT pressed(*hotkey);
    });
    connect(shortcuts, &GlobalShortcuts::keyChanged, this, [this](const QString &id) {
        if (const std::optional<Hotkey> hotkey = hotkeyOf(id))
            Q_EMIT keyChanged(*hotkey);
    });
}

void Hotkeys::add(const OverlaySettings &settings)
{
    m_shortcuts->add(idOf(Hotkey::Check), u"Check the item under the mouse"_s, sequenceOf(settings.hotkey));
    m_shortcuts->add(idOf(Hotkey::Results), u"Show the trade results for it"_s,
                     sequenceOf(settings.resultsHotkey));
}

QString Hotkeys::key(Hotkey hotkey) const
{
    return textOf(m_shortcuts->key(idOf(hotkey)));
}

bool Hotkeys::setKey(Hotkey hotkey, const QString &key)
{
    const QKeySequence sequence = sequenceOf(key);
    // a key it doesn't know reads as none
    if (!key.isEmpty() && textOf(sequence).isEmpty())
        return false;
    const QString id = idOf(hotkey);
    if (sequence == m_shortcuts->key(id))
        return true;
    if (!m_shortcuts->setKey(id, sequence))
        return false;
    Q_EMIT keyChanged(hotkey);
    return true;
}

bool Hotkeys::isAvailable() const
{
    return m_shortcuts->isAvailable();
}

QString Hotkeys::errorString() const
{
    return m_shortcuts->errorString();
}

} // namespace pou
