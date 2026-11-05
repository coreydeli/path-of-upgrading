// SPDX-License-Identifier: MIT
#include "core/places.h"

#include "core/slots.h"
#include "core/wearing.h"

#include <algorithm>

namespace pou {
namespace {

// There's something to choose between when two places have something in them.
bool worthComparing(const QList<PlaceChange> &places)
{
    return std::ranges::count_if(places, [](const PlaceChange &p) { return !p.wearing.isEmpty(); }) > 1;
}

} // namespace

QList<PlaceChange> gearPlaces(const Item &item, const Character &character, const PobPriorities &priorities,
                              const QString &bestSlot)
{
    // for the rest the verdict doesn't pick between places
    if (!comparesWithWeakest(item.slot))
        return {};

    QList<PlaceChange> places;
    for (const QString &slot : slotsFor(item)) {
        const Evaluation e = evaluate(item, character, priorities, slot);
        if (!e.comparison.ok || e.comparison.isEquipped)
            return {};
        const auto there = character.gear.constFind(slot);
        places << PlaceChange{.slot = slot,
                              .wearing = there != character.gear.cend() ? pobItemName(*there) : QString(),
                              .best = slot == bestSlot,
                              .mode = e.mode,
                              .score = e.comparison.delta};
    }
    if (!worthComparing(places))
        return {};
    return places;
}

QList<PlaceChange> pobPlaces(const PobPlacements &tried)
{
    QList<PlaceChange> places;
    for (qsizetype i = 0; i < tried.judgements.size(); ++i) {
        const PobJudgement &j = tried.judgements.at(i);
        places << PlaceChange{.slot = j.placement.label.isEmpty() ? j.placement.slot : j.placement.label,
                              .wearing = j.placement.replacing,
                              .best = i == tried.best,
                              .mode = ScoreMode::PathOfBuilding,
                              .score = j.verdict.score,
                              .damage = j.verdict.damage.percent,
                              .ehp = j.verdict.ehp ? j.verdict.ehp->percent : std::nullopt,
                              .minionDamage = j.verdict.damage.stat == u"MinionDPS"};
    }
    if (!worthComparing(places))
        return {};
    return places;
}

} // namespace pou
