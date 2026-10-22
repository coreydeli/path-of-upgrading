// SPDX-License-Identifier: MIT
#include "core/pobverdict.h"

#include "core/numbers.h"

#include <algorithm>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Scores, in percent better for the build, from which an item is an upgrade,
// a small one or a downgrade.
constexpr double upgradeAt = 3;
constexpr double smallUpgradeAt = 1;
constexpr double downgradeAt = -1;
// With keepDamage, an item that costs this many percent of your damage loses
// damage, however much resistance or effective HP it adds.
constexpr double damageLossAt = 5;
// With resistsFirst, what each resistance point toward a cap is worth.
constexpr double resistPoint = 0.5;
// Going from no damage to some, as when your weapon couldn't use the skill
// before, counts as this many percent better.
constexpr double damageFromNothing = 25;
// Path of Building rounds resistances to whole numbers.
constexpr double slack = 0.5;

std::optional<double> stat(const PobStats &stats, const QString &name)
{
    const auto it = stats.constFind(name);
    if (it == stats.cend())
        return std::nullopt;
    return *it;
}

std::optional<double> percentChange(double from, double to)
{
    if (from <= 0)
        return std::nullopt;
    return (to - from) / from * 100;
}

PobChange change(const QString &stat, double from, double to)
{
    PobChange c{stat, from, to, std::nullopt};
    if (const std::optional<double> p = percentChange(from, to))
        c.percent = roundTo(*p, 1);
    return c;
}

// Path of Building says how far under the cap a resistance is rather than
// what the cap is.
std::optional<double> resistMax(const PobStats &stats, Element e)
{
    const std::optional<double> value = stat(stats, elementName(e) + u"Resist"_s);
    const std::optional<double> missing = stat(stats, u"Missing"_s + elementName(e) + u"Resist"_s);
    if (!value || !missing)
        return std::nullopt;
    return *value + *missing;
}

// At the cap, or held down to it.
bool isCapped(double value, std::optional<double> total, double max)
{
    return value >= max - slack || (total && *total > value + slack);
}

QString whole(double v)
{
    return formatNumber(roundHalfUp(v));
}

// Returns the points the item adds toward the caps.
double judgeResists(const PobStats &before, const PobStats &after, const PobVerdictOptions &options,
                    PobVerdict &v)
{
    double gain = 0;
    for (Element e : allElements) {
        if (e == Element::Chaos && options.ignoreChaos)
            continue;
        const QString name = elementName(e) + u"Resist"_s;
        const std::optional<double> from = stat(before, name);
        const std::optional<double> to = stat(after, name);
        if (!from || !to)
            continue;

        PobResistChange r;
        r.element = e;
        r.from = *from;
        r.to = *to;
        r.totalFrom = stat(before, name + u"Total"_s);
        r.totalTo = stat(after, name + u"Total"_s);
        // the usual cap when Path of Building doesn't say
        const double maxFrom = resistMax(before, e).value_or(resistMax(after, e).value_or(75));
        r.max = resistMax(after, e).value_or(maxFrom);
        r.wasCapped = isCapped(r.from, r.totalFrom, maxFrom);
        r.capped = isCapped(r.to, r.totalTo, r.max);

        // Chaos resistance only counts when you say so, though dropping off a
        // cap you had always does.
        const bool counts = e != Element::Chaos || options.priorities.chaos;
        // Under the cap isn't enough: an item that raises one you're at leaves
        // you under it with what you had.
        const bool drops = r.to < r.from - slack;
        if (counts)
            gain += std::min(r.to, r.max) - std::min(r.from, maxFrom);
        if (r.wasCapped && !r.capped && drops) {
            r.lost = true;
            v.breaks << u"%1 res drops below the cap (%2% of %3%)"_s.arg(elementName(e), whole(r.to),
                                                                         whole(r.max));
        } else if (!r.wasCapped && drops && counts) {
            r.lost = true;
            v.breaks << u"%1 res goes further under the cap (from %2% to %3%)"_s.arg(
                elementName(e), whole(r.from), whole(r.to));
        }
        r.reachesCap = !r.wasCapped && r.capped && counts;

        if (r.from != r.to || r.totalFrom != r.totalTo)
            v.resists << r;
    }
    return gain;
}

} // namespace

QString pobDamageStat(const PobStats &stats)
{
    for (const QString &name : {u"FullDPS"_s, u"MinionDPS"_s, u"CombinedDPS"_s}) {
        if (stats.value(name) > 0)
            return name;
    }
    return u"TotalDPS"_s;
}

PobVerdict judgeWithPob(const PobStats &before, const PobStats &after, const PobVerdictOptions &options)
{
    const PobPriorities &priorities = options.priorities;
    const double damageWeight = std::clamp(options.damageWeight, 0.0, 1.0);
    PobVerdict v;

    // the same number on both sides, even when the item makes another one bigger
    const QString damageStat = pobDamageStat(before);
    v.damage = change(damageStat, before.value(damageStat), after.value(damageStat));
    std::optional<double> damagePercent = percentChange(v.damage.from, v.damage.to);
    if (v.damage.from <= 0 && v.damage.to > 0)
        damagePercent = damageFromNothing;

    std::optional<double> ehpPercent;
    const std::optional<double> ehpFrom = stat(before, u"TotalEHP"_s);
    const std::optional<double> ehpTo = stat(after, u"TotalEHP"_s);
    if (ehpFrom && ehpTo) {
        v.ehp = change(u"TotalEHP"_s, *ehpFrom, *ehpTo);
        ehpPercent = percentChange(*ehpFrom, *ehpTo);
    }

    double score = damagePercent.value_or(0) * damageWeight + ehpPercent.value_or(0) * (1 - damageWeight);

    const double resistGain = judgeResists(before, after, options, v);
    if (priorities.resistsFirst)
        score += resistGain * resistPoint;

    v.losesDamage = priorities.keepDamage && v.damage.from > 0 && damagePercent.value_or(0) <= -damageLossAt;
    if (v.losesDamage) {
        v.breaks << u"%1 drops %2%"_s.arg(damageStat == u"MinionDPS" ? u"Minion damage"_s : u"Damage"_s,
                                          formatNumber(std::abs(v.damage.percent.value_or(0))));
    }

    if (score >= upgradeAt)
        v.verdict = Verdict::Upgrade;
    else if (score >= smallUpgradeAt)
        v.verdict = Verdict::Small;
    else if (score <= downgradeAt)
        v.verdict = Verdict::Downgrade;

    const bool losesResist =
        std::any_of(v.resists.cbegin(), v.resists.cend(), [](const PobResistChange &r) { return r.lost; });
    const bool capsResist = std::any_of(v.resists.cbegin(), v.resists.cend(),
                                        [](const PobResistChange &r) { return r.reachesCap; });
    // Losing your main damage is never an upgrade, and resistances matter
    // most: either always shows as a verdict of its own.
    if (v.losesDamage)
        v.verdict = Verdict::DamageLoss;
    if (losesResist)
        v.verdict = Verdict::ResistLoss;

    v.score = roundTo(score, 1);
    v.resistGain = int(roundHalfUp(resistGain));
    if (losesResist || v.losesDamage)
        v.resistRank = -1;
    else if (capsResist)
        v.resistRank = 2;
    else if (resistGain > 0)
        v.resistRank = 1;

    for (const QString &name :
         {u"Life"_s, u"EnergyShield"_s, u"Mana"_s, u"Spirit"_s, u"SpiritUnreserved"_s, u"MinionLife"_s}) {
        const std::optional<double> from = stat(before, name);
        const std::optional<double> to = stat(after, name);
        if (from && to && roundHalfUp(*to - *from) != 0)
            v.changes << change(name, *from, *to);
    }
    return v;
}

} // namespace pou
