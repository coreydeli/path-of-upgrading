// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>

namespace pou {

// How many of the items you've copied had each stat, by item class:
// "Gloves" -> "# to maximum Life" -> 3.
using SeenStats = QHash<QString, QHash<QString, int>>;

// Counts the stats on the items you copy, for the stat picker. An item you
// copy again isn't counted twice, as long as it's one of the last couple of
// thousand.
class SeenStatsLog
{
public:
    // False when the item was counted already or has nothing to count.
    bool note(const Item &item);
    const SeenStats &stats() const { return m_stats; }

    QJsonObject toJson() const;
    static SeenStatsLog fromJson(const QJsonObject &o);

private:
    void remember(const QByteArray &fingerprint);

    SeenStats m_stats;
    QList<QByteArray> m_recent; // fingerprints of the items counted, oldest first
    QSet<QByteArray> m_counted; // the same, to look up
};

} // namespace pou
