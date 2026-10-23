// SPDX-License-Identifier: MIT
#include "app/itemreader.h"

#include "core/parser.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QString whyNothing(GameInput::Failure why, const QString &detail, const QString &hotkey)
{
    switch (why) {
    case GameInput::Failure::Unavailable:
        return detail.isEmpty() ? u"Items can't be copied out of the game here."_s : detail;
    case GameInput::Failure::NotInFront:
        if (detail.isEmpty()) {
            return u"Path of Exile 2 wasn't the active window, so it didn't copy the item. Click once in "
                   "the game, then press %1 again."_s.arg(hotkey);
        }
        return u"Path of Exile 2 wasn't the active window, %1 was, so it didn't copy the item. Click "
               "once in the game, then press %2 again."_s.arg(detail, hotkey);
    case GameInput::Failure::KeyHeld:
        return u"A key was still down after %1, so the game wasn't asked to copy. Let go of the other "
               "keys, then press %1 again."_s.arg(hotkey);
    case GameInput::Failure::NotOverGame:
        return u"Your mouse wasn't over the game. Hover the item in game, then press %1."_s.arg(hotkey);
    case GameInput::Failure::NothingCopied:
        return u"No item under the mouse. Hover an item until its tooltip shows, then press %1."_s.arg(
            hotkey);
    }
    Q_UNREACHABLE_RETURN({});
}

} // namespace

ItemReader::ItemReader(GameInput *input, QObject *parent)
    : QObject(parent)
    , m_input(input)
{
    connect(input, &GameInput::itemCopied, this, &ItemReader::copied);
    connect(input, &GameInput::copyFailed, this, &ItemReader::notCopied);
}

void ItemReader::setHotkey(const QString &key)
{
    m_hotkey = key;
}

void ItemReader::setAdvancedCopy(bool advanced)
{
    m_advanced = advanced;
}

void ItemReader::setPaused(bool paused)
{
    m_paused = paused;
}

void ItemReader::read()
{
    if (m_paused || m_reading)
        return;
    m_reading = true;
    // which can fail before it returns
    m_input->copyItem(m_advanced);
}

void ItemReader::copied(const QString &text, const std::optional<ScreenPoint> &mouse)
{
    finish();
    if (const std::optional<Item> item = parseItem(text)) {
        Q_EMIT itemRead(*item, mouse);
        return;
    }
    Q_EMIT failed(u"That wasn't an item, the game copied something else. Hover an item until its tooltip "
                  "shows, then press %1."_s.arg(hotkey()),
                  mouse);
}

void ItemReader::notCopied(GameInput::Failure why, const QString &detail,
                           const std::optional<ScreenPoint> &mouse)
{
    finish();
    Q_EMIT failed(whyNothing(why, detail, hotkey()), mouse);
}

void ItemReader::finish()
{
    m_reading = false;
}

QString ItemReader::hotkey() const
{
    return m_hotkey.isEmpty() ? u"the hotkey"_s : m_hotkey;
}

} // namespace pou
