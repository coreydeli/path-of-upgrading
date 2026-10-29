// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "core/seenstats.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <chrono>

namespace pou {

// The items you checked lately, newest first, and the stats on all you've
// checked, for the stat picker. They're kept in a file, saved a moment after
// the last item so a run of them is saved once, and when it's destroyed.
class ItemHistory : public QObject
{
    Q_OBJECT

public:
    // Reads the file. When it can't, it starts again with nothing, which is
    // saved over it.
    explicit ItemHistory(const QString &path, QObject *parent = nullptr);
    ~ItemHistory() override;

    void setSaveDelay(std::chrono::milliseconds delay);

    // The same item again moves to the front.
    void add(const Item &item);
    const QList<Item> &recent() const { return m_recent; }
    // Forgets the items but not the stats, which the stat picker keeps.
    void clearRecent();
    const SeenStats &seenStats() const { return m_seen.stats(); }

    // Saves straight away what's waiting to be.
    bool save();
    // Why the file couldn't be read, or last saved.
    QString errorString() const { return m_error; }

Q_SIGNALS:
    void changed();

private:
    void load();

    QString m_path;
    QList<Item> m_recent;
    SeenStatsLog m_seen;
    QTimer m_saveTimer; // running while there's something to save
    QString m_error;
};

} // namespace pou
