// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <chrono>

namespace pou {

// What a key held down means to a copy.
enum class Modifier {
    Control,
    Alt,
    Other,
};

struct HeldModifier
{
    quint32 key = 0;
    Modifier modifier = Modifier::Other;
};

// The keys the game's copy takes, by the keyboard's codes for them.
struct CopyKeys
{
    quint32 control = 0;
    quint32 alt = 0;
    quint32 c = 0;
};

// A key going down or up, some time after the step before.
struct KeyStep
{
    quint32 key = 0;
    bool press = false;
    std::chrono::milliseconds after{0};

    friend bool operator==(const KeyStep &, const KeyStep &) = default;
};

// The game's Ctrl+C, or Ctrl+Alt+C for its advanced copy, around the
// modifiers still held from the hotkey.
QList<KeyStep> copySteps(const CopyKeys &keys, const QList<HeldModifier> &held, bool advanced);

} // namespace pou
