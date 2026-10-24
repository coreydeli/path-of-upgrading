// SPDX-License-Identifier: MIT
#pragma once

#include "platform/globalshortcuts.h"

#include <QHash>
#include <QList>

// The desktop's shortcuts for tests. They keep the keys they had, as the
// desktop does between runs, and know the keys other programs have.
class FakeGlobalShortcuts : public pou::GlobalShortcuts
{
public:
    using pou::GlobalShortcuts::GlobalShortcuts;

    bool isAvailable() const override { return true; }
    QString errorString() const override { return {}; }

    void add(const QString &id, const QString &name, const QKeySequence &key) override
    {
        names.insert(id, name);
        if (!keys.contains(id))
            keys.insert(id, taken.contains(key) ? QKeySequence() : key);
    }

    QKeySequence key(const QString &id) const override { return keys.value(id); }

    bool setKey(const QString &id, const QKeySequence &key) override
    {
        if (!names.contains(id))
            return false;
        const bool ours = !key.isEmpty() && keys.key(key, id) != id;
        if (taken.contains(key) || ours)
            return false;
        keys.insert(id, key);
        return true;
    }

    // as in System Settings
    void change(const QString &id, const QKeySequence &key)
    {
        keys.insert(id, key);
        Q_EMIT keyChanged(id, key);
    }

    QHash<QString, QString> names; // of the ones added
    // Theirs, from before the app started when it's set first.
    QHash<QString, QKeySequence> keys;
    QList<QKeySequence> taken; // by other programs
};
