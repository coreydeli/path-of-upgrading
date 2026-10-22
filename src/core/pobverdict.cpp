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
// damage, however much effective HP it adds.
constexpr double damageLossAt = 5;
// Going from no damage to some, as when your weapon couldn't use the skill
// before, counts as this many percent better.
constexpr double damageFromNothing = 25;

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

    const double score =
        damagePercent.value_or(0) * damageWeight + ehpPercent.value_or(0) * (1 - damageWeight);

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
    // Losing your main damage is never an upgrade.
    if (v.losesDamage)
        v.verdict = Verdict::DamageLoss;
    v.score = roundTo(score, 1);

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
