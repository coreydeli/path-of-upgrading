// SPDX-License-Identifier: MIT
#include "platform/win/winkeys.h"

#include <QHash>

namespace pou {
namespace {

// from winuser.h
constexpr quint32 modAlt = 0x1;
constexpr quint32 modControl = 0x2;
constexpr quint32 modShift = 0x4;
constexpr quint32 modWin = 0x8;
// or holding the key down repeats it
constexpr quint32 modNoRepeat = 0x4000;

constexpr quint32 vkShift = 0x10;
constexpr quint32 vkMenu = 0x12;
constexpr quint32 vkLeftWin = 0x5b;
constexpr quint32 vkRightWin = 0x5c;
constexpr quint32 vkNumpad0 = 0x60;
constexpr quint32 vkF1 = 0x70;
constexpr quint32 vkLeftShift = 0xa0;
constexpr quint32 vkRightShift = 0xa1;
constexpr quint32 vkLeftControl = 0xa2;
constexpr quint32 vkRightControl = 0xa3;
constexpr quint32 vkLeftMenu = 0xa4;
constexpr quint32 vkRightMenu = 0xa5;

const QHash<int, quint32> &namedKeys()
{
    // The punctuation is where it is on a US keyboard, which is what
    // Windows names it by.
    static const QHash<int, quint32> keys = {
        {Qt::Key_Backspace, 0x08},    {Qt::Key_Tab, 0x09},         {Qt::Key_Return, 0x0d},
        {Qt::Key_Enter, 0x0d},        {Qt::Key_Pause, 0x13},       {Qt::Key_Escape, 0x1b},
        {Qt::Key_Space, 0x20},        {Qt::Key_PageUp, 0x21},      {Qt::Key_PageDown, 0x22},
        {Qt::Key_End, 0x23},          {Qt::Key_Home, 0x24},        {Qt::Key_Left, 0x25},
        {Qt::Key_Up, 0x26},           {Qt::Key_Right, 0x27},       {Qt::Key_Down, 0x28},
        {Qt::Key_Print, 0x2c},        {Qt::Key_Insert, 0x2d},      {Qt::Key_Delete, 0x2e},
        {Qt::Key_Semicolon, 0xba},    {Qt::Key_Equal, 0xbb},       {Qt::Key_Comma, 0xbc},
        {Qt::Key_Minus, 0xbd},        {Qt::Key_Period, 0xbe},      {Qt::Key_Slash, 0xbf},
        {Qt::Key_QuoteLeft, 0xc0},    {Qt::Key_BracketLeft, 0xdb}, {Qt::Key_Backslash, 0xdc},
        {Qt::Key_BracketRight, 0xdd}, {Qt::Key_Apostrophe, 0xde},
    };
    return keys;
}

const QHash<int, quint32> &keypadKeys()
{
    static const QHash<int, quint32> keys = {
        {Qt::Key_Asterisk, 0x6a}, {Qt::Key_Plus, 0x6b},  {Qt::Key_Minus, 0x6d},
        {Qt::Key_Period, 0x6e},   {Qt::Key_Slash, 0x6f}, {Qt::Key_Enter, 0x0d},
    };
    return keys;
}

std::optional<quint32> virtualKey(Qt::Key key, bool keypad)
{
    if (keypad && key >= Qt::Key_0 && key <= Qt::Key_9)
        return vkNumpad0 + quint32(key - Qt::Key_0);
    if (keypad) {
        if (const auto found = keypadKeys().constFind(key); found != keypadKeys().cend())
            return *found;
    }
    // Windows' codes for these are their capital letters and digits
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9))
        return quint32(key);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return vkF1 + quint32(key - Qt::Key_F1);
    if (const auto found = namedKeys().constFind(key); found != namedKeys().cend())
        return *found;
    return std::nullopt;
}

} // namespace

std::optional<WinHotKey> winHotKey(QKeyCombination combination)
{
    const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    const std::optional<quint32> key = virtualKey(combination.key(), modifiers.testFlag(Qt::KeypadModifier));
    if (!key)
        return std::nullopt;
    WinHotKey hotKey = {.modifiers = modNoRepeat, .key = *key};
    if (modifiers.testFlag(Qt::AltModifier))
        hotKey.modifiers |= modAlt;
    if (modifiers.testFlag(Qt::ControlModifier))
        hotKey.modifiers |= modControl;
    if (modifiers.testFlag(Qt::ShiftModifier))
        hotKey.modifiers |= modShift;
    if (modifiers.testFlag(Qt::MetaModifier))
        hotKey.modifiers |= modWin;
    return hotKey;
}

std::optional<Modifier> winModifier(quint32 key)
{
    switch (key) {
    case vkLeftControl:
    case vkRightControl:
        return Modifier::Control;
    case vkLeftMenu:
    case vkRightMenu:
        return Modifier::Alt;
    case vkLeftShift:
    case vkRightShift:
    case vkLeftWin:
    case vkRightWin:
        return Modifier::Other;
    default:
        return std::nullopt;
    }
}

bool winCountsAsHeld(quint32 key)
{
    // the mouse buttons are 1, 2 and 4 to 6, and Ctrl is between Shift and Alt
    const bool mouse = key == 0x1 || key == 0x2 || (key >= 0x4 && key <= 0x6);
    return !mouse && (key < vkShift || key > vkMenu);
}

CopyKeys winCopyKeys()
{
    return {.control = vkLeftControl, .alt = vkLeftMenu, .c = quint32(Qt::Key_C)};
}

} // namespace pou
