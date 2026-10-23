// SPDX-License-Identifier: MIT
#include "platform/copykeys.h"

#include <algorithm>

namespace pou {

using namespace std::chrono_literals;

QList<KeyStep> copySteps(const CopyKeys &keys, const QList<HeldModifier> &held, bool advanced)
{
    const QList<Modifier> wanted =
        advanced ? QList{Modifier::Control, Modifier::Alt} : QList{Modifier::Control};
    const auto isHeld = [&held](Modifier modifier) {
        return std::ranges::any_of(held,
                                   [modifier](const HeldModifier &key) { return key.modifier == modifier; });
    };

    QList<KeyStep> steps;
    // the others go up first, or Shift still held from the hotkey would make
    // it Ctrl+Shift+C
    for (const HeldModifier &key : held) {
        if (!wanted.contains(key.modifier))
            steps << KeyStep{.key = key.key, .press = false};
    }
    QList<quint32> pressed;
    for (const Modifier modifier : wanted) {
        if (isHeld(modifier))
            continue;
        const quint32 key = modifier == Modifier::Control ? keys.control : keys.alt;
        steps << KeyStep{.key = key, .press = true, .after = pressed.isEmpty() ? 0ms : 15ms};
        pressed << key;
    }
    // The game looks at the keyboard once a frame and misses a key that goes
    // down and up in between, so each stays put for about a frame, and C for
    // two.
    steps << KeyStep{.key = keys.c, .press = true, .after = 15ms}
          << KeyStep{.key = keys.c, .press = false, .after = 40ms};
    for (auto key = pressed.crbegin(); key != pressed.crend(); ++key)
        steps << KeyStep{.key = *key, .press = false, .after = 15ms};
    return steps;
}

} // namespace pou
