// SPDX-License-Identifier: MIT
#include "core/recommendations.h"

#include "core/evaluation.h"
#include "core/hands.h"
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

} // namespace

QList<WantedStat> wantedStats(const QString &slot, const Item *worn, const GearScoreOptions &options,
                              int count)
{
    // the same bar topStats() uses for the stats worth searching for
    constexpr double worthSearching = 4;
    QList<SearchWeight> weights;
    for (const SearchWeight &w : searchWeights(slot, options)) {
        if (!w.key.isEmpty() && w.importance >= worthSearching)
            weights << w;
    }
    std::ranges::stable_sort(weights, std::greater<>(), &SearchWeight::importance);

    const SlotMods &mods = options.mods ? *options.mods : SlotMods::builtIn();
    const QString itemClass = worn && !worn->itemClass.isEmpty() ? worn->itemClass : classOfSlot(slot);
    QList<WantedStat> out;
    for (const SearchWeight &w : std::as_const(weights)) {
        double minimum = searchMinimum(w.cap);
        if (w.need)
            minimum = std::min(minimum, std::ceil(*w.need));
        if (worn && rolledOn(*worn, w.pattern) >= minimum)
            continue;
        const auto top = mods.best(itemClass, w.key);
        out << WantedStat{.label = w.label, .minimum = minimum, .best = top ? top->high : 0};
        if (out.size() == count)
            break;
    }
    return out;
}

QString wantedText(const QList<WantedStat> &stats)
{
    QStringList parts;
    for (const WantedStat &s : stats)
        parts << s.label + u' ' + formatNumber(s.minimum) + u" or more"_s +
                     (s.best > 0 ? u" (up to "_s + rollText(s.best) + u')' : QString());
    return parts.join(u", "_s);
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
