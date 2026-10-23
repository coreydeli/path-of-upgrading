// SPDX-License-Identifier: MIT
#pragma once

#include <QKeySequence>
#include <QObject>
#include <memory>

namespace pou {

// Keys that work whichever window has the keyboard, which while you play is
// the game. The desktop keeps them, so on KDE they can be changed in System
// Settings as well as in ours.
class GlobalShortcuts : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual bool isAvailable() const = 0;
    virtual QString errorString() const = 0;

    // By an id that mustn't change between versions, with a name for it in
    // System Settings. It gets the key it had last time, or this one the
    // first time.
    virtual void add(const QString &id, const QString &name, const QKeySequence &key) = 0;
    // None when it has no key, as when another program had it first.
    virtual QKeySequence key(const QString &id) const = 0;
    // Whether it has the key now. When another program has it, or another of
    // ours does, it keeps the one it had.
    virtual bool setKey(const QString &id, const QKeySequence &key) = 0;

Q_SIGNALS:
    void activated(const QString &id);
    // Also when it's changed outside the app, as in System Settings.
    void keyChanged(const QString &id, const QKeySequence &key);
};

// The ones for this desktop.
std::unique_ptr<GlobalShortcuts> createGlobalShortcuts();

} // namespace pou
