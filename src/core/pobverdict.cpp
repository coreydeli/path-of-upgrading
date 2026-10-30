// SPDX-License-Identifier: MIT
#include "core/pobverdict.h"

#include "core/numbers.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
        // Being under the cap isn't a loss by itself, the resistance has to go down.
        const bool drops = r.to < r.from - slack;
        if (counts)
            gain += std::min(r.to, r.max) - std::min(r.from, maxFrom);
        if (r.wasCapped && !r.capped && drops) {
            r.lost = true;
            v.losses << u"%1 res drops below the cap (%2% of %3%)"_s.arg(elementName(e), whole(r.to),
                                                                         whole(r.max));
        } else if (!r.wasCapped && drops && counts) {
            r.lost = true;
            v.losses << u"%1 res goes further under the cap (from %2% to %3%)"_s.arg(
                elementName(e), whole(r.from), whole(r.to));
        }
        r.reachesCap = !r.wasCapped && r.capped && counts;

        if (r.from != r.to || r.totalFrom != r.totalTo)
            v.resists << r;
    }
    return gain;
}

void judgeRequirements(const PobStats &before, const PobStats &after, const PobPriorities &priorities,
                       PobVerdict &v)
{
    const std::optional<double> spiritFrom = stat(before, u"SpiritUnreserved"_s);
    const std::optional<double> spiritTo = stat(after, u"SpiritUnreserved"_s);
    if (priorities.spirit && spiritFrom && spiritTo && *spiritTo < 0 && *spiritTo < *spiritFrom)
        v.breaks << u"Not enough Spirit for your minions and auras (%1 short)"_s.arg(whole(-*spiritTo));

    if (!priorities.attributes)
        return;
    static const QList<std::pair<QString, QString>> attributes = {
        {u"Str"_s, u"Strength"_s},
        {u"Dex"_s, u"Dexterity"_s},
        {u"Int"_s, u"Intelligence"_s},
    };
    for (const auto &[attribute, label] : attributes) {
        const std::optional<double> had = stat(before, attribute);
        const std::optional<double> needed = stat(before, u"Req"_s + attribute);
        const std::optional<double> have = stat(after, attribute);
        const std::optional<double> need = stat(after, u"Req"_s + attribute);
        // gear you couldn't wear before isn't the item's fault
        const bool hadEnough = !had || !needed || *had >= *needed;
        if (have && need && *have < *need && hadEnough)
            v.breaks << u"Not enough %1 to use your gear (%2 of %3)"_s.arg(label, whole(*have), whole(*need));
    }
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
        v.losses << u"%1 drops %2%"_s.arg(damageStat == u"MinionDPS" ? u"Minion damage"_s : u"Damage"_s,
                                          formatNumber(std::abs(v.damage.percent.value_or(0))));
    }
    judgeRequirements(before, after, priorities, v);
    v.breaks = v.losses + v.breaks;

    if (score >= upgradeAt)
        v.verdict = Verdict::Upgrade;
    else if (score >= smallUpgradeAt)
        v.verdict = Verdict::Small;
    else if (score <= downgradeAt)
        v.verdict = Verdict::Downgrade;
    if (!v.breaks.isEmpty())
        v.verdict = score > 0 ? Verdict::Risky : Verdict::Downgrade;

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

qsizetype bestVerdict(const QList<PobVerdict> &verdicts)
{
    const auto best =
        std::min_element(verdicts.cbegin(), verdicts.cend(), [](const PobVerdict &a, const PobVerdict &b) {
            if (a.breaks.size() != b.breaks.size())
                return a.breaks.size() < b.breaks.size();
            return a.score > b.score;
        });
    return best == verdicts.cend() ? -1 : best - verdicts.cbegin();
}

QList<Element> resistsUnderCap(const PobStats &stats, const PobVerdictOptions &options)
{
    QList<Element> out;
    for (Element e : allElements) {
        if (e == Element::Chaos && (options.ignoreChaos || !options.priorities.chaos))
            continue;
        const QString name = elementName(e) + u"Resist"_s;
        const std::optional<double> value = stat(stats, name);
        if (value && !isCapped(*value, stat(stats, name + u"Total"_s), resistMax(stats, e).value_or(75)))
            out << e;
    }
    return out;
}

bool hasChaosInoculation(const PobStats &stats)
{
    if (const std::optional<double> flag = stat(stats, u"ChaosInoculation"_s))
        return *flag != 0;
    const double life = stats.value(u"Life"_s);
    return life > 0 && life <= 1;
}

} // namespace pou
