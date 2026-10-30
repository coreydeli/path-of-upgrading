// SPDX-License-Identifier: MIT
#pragma once

#include "platform/screenmap.h"

#include <QObject>
#include <memory>
#include <optional>

namespace pou {

// Getting items out of the game, which copies an item's text when you press
// Ctrl+C over it. This presses that for the hotkey, and hears about the copies
// you make yourself.
class GameInput : public QObject
{
    Q_OBJECT

public:
    // Why the game didn't copy an item.
    enum class Failure {
        Unavailable,   // keys can't be pressed in the game here, and the detail says why
        NotInFront,    // the game isn't in front, nor under the mouse to bring it there
        KeyHeld,       // a key stayed down after the hotkey
        NotOverGame,   // nothing was copied, and the mouse wasn't over the game
        NothingCopied, // nothing was copied with the mouse over the game
    };
    Q_ENUM(Failure)

    struct CopyOptions
    {
        // with Alt as well, which has the mods' tiers in the copy
        bool advanced = false;
        // The popup was over the mouse and is going, so the game is waiting for
        // it to be gone and then given a nudge of the mouse. Until it moves,
        // the game still has the popup under it and no item.
        bool nudge = false;
    };

    using QObject::QObject;

    virtual bool isAvailable() const = 0;
    virtual QString errorString() const = 0;

    // Has the game copy the item under the mouse, which ends in itemCopied or
    // copyFailed. While a copy is under way, another does nothing.
    virtual void copyItem(const CopyOptions &options) = 0;
    // Where the mouse is, when it can be told, as it can over the game.
    virtual std::optional<ScreenPoint> mouse() const = 0;

Q_SIGNALS:
    // With where the mouse was, when that can be told.
    void itemCopied(const QString &text, const std::optional<ScreenPoint> &mouse);
    // The detail is the window in front for NotInFront, when it has a name.
    void copyFailed(GameInput::Failure why, const QString &detail, const std::optional<ScreenPoint> &mouse);
    // Whatever else gets copied, by anything.
    void textCopied(const QString &text);
};

// The one for this desktop.
std::unique_ptr<GameInput> createGameInput();

} // namespace pou
