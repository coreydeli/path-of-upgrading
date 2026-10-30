// SPDX-License-Identifier: MIT
#include "core/scorer.h"

#include "core/numbers.h"
#include "core/slots.h"

#include <algorithm>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

const Item *itemIn(const Equipped &equipped, const QString &slot)
{
    const auto it = equipped.constFind(slot);
    return it == equipped.cend() ? nullptr : &*it;
}

StatMap mergeStats(const QList<const Item *> &items)
{
    StatMap out;
    for (const Item *item : items) {
        if (!item)
            continue;
        for (const auto &[key, value] : item->stats)
            out.add(key, value);
    }
    return out;
}

Comparison compareWith(const Item &item, const QList<const Item *> &baseline, const Weights &weights,
                       const QString &targetSlot, const StatFilter &ignore)
{
    const StatMap oldStats = mergeStats(baseline);

    Comparison r;
    r.targetSlot = targetSlot;
    for (const Item *b : baseline) {
        if (b)
            r.replacing << b->name;
    }
    r.newScore = score(item.stats, weights);
    r.oldScore = score(oldStats, weights);
    r.delta = r.newScore - r.oldScore;
    if (r.oldScore != 0)
        r.percent = r.delta / std::abs(r.oldScore) * 100;
    r.rows = diffStats(item.stats, oldStats, weights, ignore);
    return r;
}

// Is the copied item the one already saved?
bool sameItem(const Item &a, const Item *b)
{
    if (!b || a.name != b->name || a.itemClass != b->itemClass || a.lines.size() != b->lines.size())
        return false;
    for (qsizetype i = 0; i < a.lines.size(); ++i) {
        if (a.lines.at(i).text != b->lines.at(i).text)
            return false;
    }
    return true;
}

} // namespace

double score(const StatMap &stats, const Weights &weights)
{
    double total = 0;
    for (const auto &[key, value] : stats)
        total += weights.value(key) * value;
    return total;
}

QList<StatRow> diffStats(const StatMap &newStats, const StatMap &oldStats, const Weights &weights,
                         const StatFilter &ignore)
{
    QStringList keys = newStats.keys();
    for (const auto &[key, value] : oldStats) {
        if (!newStats.contains(key))
            keys << key;
    }

    QList<StatRow> rows;
    for (const QString &key : std::as_const(keys)) {
        if (ignore && ignore(key))
            continue;
        const double newValue = newStats.value(key);
        const double oldValue = oldStats.value(key);
        const double diff = roundTo(newValue - oldValue, 2);
        if (diff == 0)
            continue;
        const double weight = weights.value(key);
        rows << StatRow{key, newValue, oldValue, diff, weight, diff * weight, weight};
    }
    sortRows(rows);
    return rows;
}

void sortRows(QList<StatRow> &rows)
{
    // those worth nothing sort by size after the rest
    std::stable_sort(rows.begin(), rows.end(), [](const StatRow &a, const StatRow &b) {
        const double worthA = std::abs(a.diff * a.usefulness);
        const double worthB = std::abs(b.diff * b.usefulness);
        if (worthA != worthB)
            return worthA > worthB;
        return std::abs(a.diff) > std::abs(b.diff);
    });
}

QString unweightedNote()
{
    return u"None of the stats that change have a weight yet."_s;
}

Comparison compare(const Item &item, const Equipped &equipped, const Weights &allWeights,
                   const StatFilter &ignore, const QString &slot)
{
    if (item.slot.isEmpty()) {
        Comparison r;
        r.reason = (item.itemClass.isEmpty() ? u"This item"_s : item.itemClass) +
                   u" isn't gear, so there's nothing to compare."_s;
        return r;
    }

    Weights weights = allWeights;
    if (ignore)
        weights.removeIf([&ignore](Weights::iterator it) { return ignore(it.key()); });

    QStringList notes;
    if (item.unidentified)
        notes << u"Unidentified: its mods are hidden, so the score only covers the base."_s;
    if (item.corrupted)
        notes << u"Corrupted, so it can't be modified any further."_s;

    const QStringList slotNames = slotGroup(item.slot);
    const auto worn = std::find_if(slotNames.cbegin(), slotNames.cend(), [&](const QString &name) {
        return sameItem(item, itemIn(equipped, name));
    });

    Comparison r;
    if (slot.isEmpty() && worn != slotNames.cend()) {
        r = compareWith(item, {itemIn(equipped, *worn)}, weights, *worn, ignore);
    } else if (item.slot == u"Weapon" && item.twoHanded && (slot.isEmpty() || groupOf(slot) == u"Weapon")) {
        const QString weaponSlot = slot.isEmpty() ? u"Weapon"_s : slot;
        const QString offhandSlot = otherHand(weaponSlot);
        r = compareWith(item, {itemIn(equipped, weaponSlot), itemIn(equipped, offhandSlot)}, weights,
                        weaponSlot, ignore);
        if (equipped.contains(offhandSlot))
            notes << u"Two-handed, so it's compared against your weapon and offhand together."_s;
    } else if (!slot.isEmpty()) {
        r = compareWith(item, {itemIn(equipped, slot)}, weights, slot, ignore);
    } else if (comparesWithWeakest(item.slot)) {
        const QStringList group = slotGroup(item.slot);
        const auto empty = std::find_if(group.cbegin(), group.cend(),
                                        [&equipped](const QString &s) { return !equipped.contains(s); });
        if (empty != group.cend()) {
            r = compareWith(item, {}, weights, *empty, ignore);
        } else {
            for (const QString &member : group) {
                Comparison c = compareWith(item, {itemIn(equipped, member)}, weights, member, ignore);
                if (member == group.first() || c.delta > r.delta)
                    r = std::move(c);
            }
        }
    } else {
        r = compareWith(item, {itemIn(equipped, item.slot)}, weights, item.slot, ignore);
    }
    if (item.slot == u"Offhand" && !item.itemClass.contains(u"quiver", Qt::CaseInsensitive)) {
        const Item *weapon = itemIn(equipped, otherHand(r.targetSlot));
        if (weapon && weapon->twoHanded)
            notes << u"Your weapon is two-handed, so you can't use this with it."_s;
    }

    r.isEquipped = sameItem(item, itemIn(equipped, r.targetSlot));

    const bool hasBaseline = !r.replacing.isEmpty();
    if (!hasBaseline)
        notes << u"Nothing saved for %1 yet. Check the item you wear there and click Wear as %1."_s.arg(
            r.targetSlot);
    const bool anyWeighted =
        std::any_of(r.rows.cbegin(), r.rows.cend(), [](const StatRow &row) { return row.weight != 0; });
    if (!anyWeighted && !r.rows.isEmpty())
        notes << unweightedNote();

    if (r.isEquipped)
        r.verdict = Verdict::Equipped;
    else if (!hasBaseline)
        r.verdict = Verdict::New;
    else if (std::abs(r.delta) < 1e-9)
        r.verdict = Verdict::Same;
    else
        r.verdict = r.delta > 0 ? Verdict::Upgrade : Verdict::Downgrade;

    r.ok = true;
    r.notes = notes;
    return r;
}

} // namespace pou
