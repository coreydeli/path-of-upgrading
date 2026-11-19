// SPDX-License-Identifier: MIT
#include "core/recommendations.h"

#include "core/evaluation.h"
#include "core/hands.h"
#include "core/modtiers.h"
#include "core/numbers.h"
#include "core/slots.h"
#include "core/statsources.h"

#include <algorithm>
#include <cmath>
#include <functional>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// How much of a stat the item rolls itself. Runes can be swapped out, so they
// don't count.
double rolledOn(const Item &item, const QRegularExpression &pattern)
{
    double total = 0;
    for (const StatSource &x : statSources(item)) {
        if ((x.origin == StatOrigin::Mod || x.origin == StatOrigin::Implicit) &&
            pattern.match(x.key).hasMatch())
            total += x.value;
    }
    return total;
}

// The stats worth searching for in a slot, most important first. A stat the
// item class can't roll as a prefix or suffix is no use to look for, whatever
// the build rates it.
QList<SearchWeight> wantedWeights(const QString &slot, const QString &itemClass,
                                  const GearScoreOptions &options)
{
    const SlotMods &mods = options.mods ? *options.mods : SlotMods::builtIn();
    // the same bar topStats() uses for the stats worth searching for
    constexpr double worthSearching = 4;
    QList<SearchWeight> weights;
    for (const SearchWeight &w : searchWeights(slot, options)) {
        if (!w.key.isEmpty() && w.importance >= worthSearching && mods.allows(itemClass, w.key))
            weights << w;
    }
    std::ranges::stable_sort(weights, std::greater<>(), &SearchWeight::importance);
    return weights;
}

} // namespace

QList<WantedStat> wantedStats(const QString &slot, const Item *worn, const GearScoreOptions &options,
                              int count)
{
    const SlotMods &mods = options.mods ? *options.mods : SlotMods::builtIn();
    const QString itemClass = worn && !worn->itemClass.isEmpty() ? worn->itemClass : classOfSlot(slot);
    const QList<SearchWeight> weights = wantedWeights(slot, itemClass, options);
    QList<WantedStat> out;
    for (const SearchWeight &w : std::as_const(weights)) {
        double minimum = searchMinimum(w.cap);
        if (w.need)
            minimum = std::min(minimum, std::ceil(*w.need));
        if (worn && rolledOn(*worn, w.pattern) >= minimum)
            continue;
        const auto top = mods.best(itemClass, w.key);
        out << WantedStat{.label = w.label,
                          .minimum = minimum,
                          .best = top ? top->high : 0,
                          .tier = tierFor(mods, itemClass, w.key, minimum).value_or(0),
                          .kind = mods.kind(itemClass, w.key)};
        if (out.size() == count)
            break;
    }
    return out;
}

QStringList wantedParts(const QList<WantedStat> &stats)
{
    QStringList parts;
    for (const WantedStat &s : stats)
        parts << s.label + u' ' + formatNumber(s.minimum) + u" or more"_s +
                     (s.best > 0 ? u" (up to "_s + rollText(s.best) +
                                       (s.tier > 0 ? u", tier %1 or better"_s.arg(s.tier) : QString()) + u')'
                                 : QString());
    return parts;
}

QStringList wantedKinds(const QList<WantedStat> &stats)
{
    QStringList kinds;
    for (const WantedStat &s : stats)
        kinds << affixName(s.kind);
    return kinds;
}

QString wantedText(const QList<WantedStat> &stats)
{
    return wantedParts(stats).join(u", "_s);
}

QList<RollRange> canRoll(const QString &slot, const Item *worn, const GearScoreOptions &options, int count)
{
    const SlotMods &mods = options.mods ? *options.mods : SlotMods::builtIn();
    const QString itemClass = worn && !worn->itemClass.isEmpty() ? worn->itemClass : classOfSlot(slot);
    QList<RollRange> out;
    for (const SearchWeight &w : wantedWeights(slot, itemClass, options)) {
        if (const auto top = mods.best(itemClass, w.key))
            out << RollRange{.label = w.label, .top = *top, .kind = mods.kind(itemClass, w.key)};
        if (out.size() == count)
            break;
    }
    return out;
}

QString rollRangeText(const RollRange &range)
{
    const ModTier &t = range.top;
    const QString roll = t.low == t.high ? rollText(t.high) : rollText(t.low) + u" to "_s + rollText(t.high);
    return range.label + u' ' + roll + u" at item level "_s + QString::number(t.level);
}

QStringList rollKinds(const QList<RollRange> &ranges)
{
    QStringList kinds;
    for (const RollRange &r : ranges)
        kinds << affixName(r.kind);
    return kinds;
}

QList<Recommendation> recommendedUpgrades(const Character &character, const PobPriorities &priorities,
                                          int count)
{
    QList<Recommendation> empty;
    QList<Recommendation> worn;
    for (const QString &slot : mainSlots()) {
        const auto item = character.gear.constFind(slot);
        const GearScoreOptions options = gearScoreOptions(character, slot, priorities);
        Recommendation r{.slot = slot};
        if (item == character.gear.cend()) {
            if (offhandOutOfUse(character.gear, character.wielding, slot))
                continue;
            r.wants = wantedStats(slot, nullptr, options);
            if (!r.wants.isEmpty())
                empty << r;
            continue;
        }
        const std::optional<GearScore> gear = gearScore(*item, options);
        if (!gear || !gear->isRated())
            continue;
        r.score = gear->score;
        r.grade = gear->grade;
        r.wants = wantedStats(slot, &*item, options);
        if (!r.wants.isEmpty())
            worn << r;
    }
    // the main slots' order settles a tie
    std::ranges::stable_sort(worn, {}, &Recommendation::score);

    QList<Recommendation> out = empty + worn;
    return out.first(std::min<qsizetype>(count, out.size()));
}

} // namespace pou
