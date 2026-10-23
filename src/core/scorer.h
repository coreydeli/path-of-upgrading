// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"
#include "core/verdict.h"
#include "core/weights.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

namespace pou {

// True for stats you've chosen to ignore: they don't count towards a score
// and aren't listed.
using StatFilter = std::function<bool(const QString &key)>;

// How one stat changes when you swap items.
struct StatRow
{
    QString key;
    double newValue = 0;
    double oldValue = 0;
    double diff = 0; // rounded to two decimals
    double weight = 0;
    double impact = 0; // diff * weight
    // What a unit of it is worth: your weight for it, or to your build
    // profile when an evaluation judges by gear score.
    double usefulness = 0;
};

struct Comparison
{
    bool ok = false;
    QString reason; // why there's nothing to compare, when not ok
    Verdict verdict = Verdict::New;
    QString targetSlot;
    QStringList replacing; // names of the items it would replace
    double newScore = 0;
    double oldScore = 0;
    double delta = 0;
    std::optional<double> percent; // delta against the old score, unknown when that was 0
    QList<StatRow> rows;
    QStringList notes;
    bool isEquipped = false; // the copied item is the one saved in that slot
};

double score(const StatMap &stats, const Weights &weights);
inline double score(const Item &item, const Weights &weights)
{
    return score(item.stats, weights);
}

// The stats that change, biggest weighted impact first.
QList<StatRow> diffStats(const StatMap &newStats, const StatMap &oldStats, const Weights &weights,
                         const StatFilter &ignore = {});

// Sorts rows by what their change is worth, diff * usefulness, biggest first.
void sortRows(QList<StatRow> &rows);

// Compares an item with what's saved in its slot. Rings, charms and jewels go
// into an empty slot if there is one, else replace your weakest; two-handed
// weapons replace the weapon and offhand together. An item you wear is
// compared with itself, where you wear it. Name a slot to compare it with
// what's there instead.
Comparison compare(const Item &item, const Equipped &equipped, const Weights &weights,
                   const StatFilter &ignore = {}, const QString &slot = {});

} // namespace pou
