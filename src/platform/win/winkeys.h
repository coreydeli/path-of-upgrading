// SPDX-License-Identifier: MIT
#pragma once

#include "platform/copykeys.h"

#include <QKeyCombination>
#include <optional>

namespace pou {

// Windows' keys by its virtual key codes, which don't need Windows to work
// out.

// What RegisterHotKey takes for a key.
struct WinHotKey
{
    quint32 modifiers = 0;
    quint32 key = 0;

    friend bool operator==(const WinHotKey &, const WinHotKey &) = default;
};

// None for a key it has no code for, or one with no key but modifiers.
std::optional<WinHotKey> winHotKey(QKeyCombination combination);

// What a modifier held down means to a copy, and none for other keys.
std::optional<Modifier> winModifier(quint32 key);
// Whether a key that's down has to go up before the copy, or is a modifier
// that the copy works around. Mouse buttons don't count, nor Shift, Ctrl and
// Alt by themselves, as Windows says which side is down as well.
bool winCountsAsHeld(quint32 key);

// The left Ctrl and Alt, and C.
CopyKeys winCopyKeys();

} // namespace pou
