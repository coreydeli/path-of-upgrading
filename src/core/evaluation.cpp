// SPDX-License-Identifier: MIT
#include "core/evaluation.h"

#include "core/numbers.h"
#include "core/slots.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Gear score points from which an item is an upgrade, a small one or a
// downgrade. A few points either way is noise.
constexpr int upgradeAt = 6;
constexpr int smallUpgradeAt = 3;
constexpr int downgradeAt = -3;

Verdict gearVerdict(int delta)
{
    if (delta >= upgradeAt)
        return Verdict::Upgrade;
    if (delta >= smallUpgradeAt)
        return Verdict::Small;
    if (delta <= downgradeAt)
        return Verdict::Downgrade;
    return Verdict::Same;
}

// what the build profiles call it
QString profileLabel(Element e)
{
    return elementName(e) + u" res"_s;
}

double capOf(const LiveResists &resists, Element e)
{
    return resists.caps[e] != 0 ? resists.caps[e] : resists.cap;
}

// Chaos resistance counts when you say so or you had it capped, and never
// with Chaos Inoculation.
bool resistCounts(const ResistChange &r, const Character &character, const PobPriorities &priorities)
{
    return r.element != Element::Chaos || (!character.chaosInoculation && (priorities.chaos || !r.wasUnder));
}

double deltaOf(const CharacterChange &change, QStringView label)
{
    for (const TotalChange &t : change.totals) {
        if (t.label == label)
            return t.delta;
    }
    return 0;
}

// Of your rings, charms or jewels, the one with the lowest gear score. One
// without a gear score, like a unique, is never it. Empty when a slot is
// free, as the item goes there first, or none of them has a gear score.
std::optional<QString> weakestByGearScore(const Item &item, const Character &character,
                                          const PobPriorities &priorities)
{
    std::optional<QString> weakest;
    int lowest = 0;
    for (const QString &member : slotGroup(item.slot)) {
        const auto there = character.gear.constFind(member);
        if (there == character.gear.cend())
            return std::nullopt;
        const std::optional<GearScore> g = gearScore(*there, gearScoreOptions(character, member, priorities));
        if (!g || !g->isRated())
            continue;
        if (!weakest || g->score < lowest) {
            weakest = member;
            lowest = g->score;
        }
    }
    return weakest;
}

} // namespace

GearScoreOptions gearScoreOptions(const Character &character, const QString &slot,
                                  const PobPriorities &priorities)
{
    GearScoreOptions options;
    options.build = character.buildProfile;
    options.tweaks = character.tweaks;
    options.ignore = [rules = character.ignored](const QString &key) {
        return isIgnored(rules, key);
    };
    if (character.chaosInoculation && !options.tweaks.contains(profileLabel(Element::Chaos)))
        options.tweaks.insert(profileLabel(Element::Chaos), 0);

    if (!character.resists)
        return options;
    const std::optional<LiveResists> resists = liveResists(*character.resists, character.gear);
    if (!resists)
        return options;
    const auto there = character.gear.constFind(slot);
    for (Element e : allElements) {
        const std::optional<double> total = resists->values[e];
        if (!total)
            continue;
        const double cap = capOf(*resists, e);
        // what's in the slot now goes when the item takes its place
        const double fromSlot = there != character.gear.cend() ? there->stats.value(resistKey(e)) : 0;
        options.resistNeed[e] = std::max(0.0, cap + resists->buffer - (*total - fromSlot));

        const bool counts = e != Element::Chaos || (priorities.chaos && !character.chaosInoculation);
        if (priorities.resistsFirst && counts && *total < cap && !options.tweaks.contains(profileLabel(e)))
            options.tweaks.insert(profileLabel(e), 2);
    }
    return options;
}

Evaluation evaluate(const Item &item, const Character &character, const PobPriorities &priorities,
                    const QString &slot)
{
    const StatFilter ignore = [&character](const QString &key) {
        return isIgnored(character.ignored, key);
    };
    Evaluation e;
    e.comparison = compare(item, character.gear, character.weights, ignore, slot);
    Comparison &r = e.comparison;
    if (!r.ok)
        return e;

    const auto gearIn = [&](const Item &it, const QString &s) {
        return gearScore(it, gearScoreOptions(character, s, priorities));
    };
    e.gear = gearIn(item, r.targetSlot);
    if (e.gear && e.gear->isRated() && slot.isEmpty() && !r.isEquipped && comparesWithWeakest(item.slot)) {
        const std::optional<QString> weakest = weakestByGearScore(item, character, priorities);
        if (weakest && *weakest != r.targetSlot) {
            r = compare(item, character.gear, character.weights, ignore, *weakest);
            e.gear = gearIn(item, r.targetSlot);
        }
    }
    const auto yours = character.gear.constFind(r.targetSlot);
    if (!r.isEquipped && yours != character.gear.cend())
        e.gearEquipped = gearIn(*yours, r.targetSlot);

    // what you wear could be a unique, which leaves only your weights
    if (e.gear && e.gear->isRated() && (!e.gearEquipped || e.gearEquipped->isRated())) {
        e.mode = ScoreMode::GearScores;
        r.newScore = e.gear->score;
        if (r.isEquipped)
            r.oldScore = r.newScore;
        else
            r.oldScore = e.gearEquipped ? e.gearEquipped->score : 0;
        r.delta = r.newScore - r.oldScore;
        r.percent.reset();
        if (!r.isEquipped && e.gearEquipped)
            r.verdict = gearVerdict(int(r.delta));
    }
    if (r.isEquipped)
        return e;

    QStringList targets = {r.targetSlot};
    if (item.twoHanded && r.targetSlot == u"Weapon" && character.gear.contains(u"Offhand"_s))
        targets << u"Offhand"_s;
    const std::optional<LiveResists> resists =
        character.resists ? liveResists(*character.resists, character.gear) : std::nullopt;
    const GearScoreFn slotScore = [&](const Item &it, const QString &s) -> std::optional<double> {
        if (const std::optional<GearScore> g = gearIn(it, s); g && g->isRated())
            return g->score;
        return std::nullopt;
    };
    e.change = changeIfEquipped(character.gear, item, targets, resists ? &*resists : nullptr, slotScore);
    if (!e.change)
        return e;

    if (const double spirit = deltaOf(*e.change, u"Spirit"); spirit < 0 && priorities.spirit)
        e.risks << u"Spirit drops by %1, so check your minions and auras still fit"_s.arg(
            formatNumber(-spirit));
    for (const ResistChange &rc : std::as_const(e.change->resists)) {
        if (!resistCounts(rc, character, priorities))
            continue;
        if (!rc.warning.isEmpty())
            e.risks << rc.warning;
        if (rc.under && rc.delta < 0)
            e.resistLosses << rc;
        else if (rc.wasUnder && !rc.under)
            e.resistRank = std::max(e.resistRank, 2);
        else if (rc.wasUnder && rc.delta > 0)
            e.resistRank = std::max(e.resistRank, 1);
    }
    if (const double levels = deltaOf(*e.change, u"Minion skill levels"); levels < 0) {
        e.risks << (levels == -1 ? u"Minion skills lose a level"_s
                                 : u"Minion skills lose %1 levels"_s.arg(formatNumber(-levels)));
    }
    if (!e.risks.isEmpty() && (r.verdict == Verdict::Upgrade || r.verdict == Verdict::Small))
        r.verdict = Verdict::Risky;
    // resistances matter most
    if (!e.resistLosses.isEmpty()) {
        r.verdict = Verdict::ResistLoss;
        e.resistRank = -1;
    }
    return e;
}

} // namespace pou
