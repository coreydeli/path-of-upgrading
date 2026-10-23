// SPDX-License-Identifier: MIT
#include "core/evaluation.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// what the build profiles call it
QString profileLabel(Element e)
{
    return elementName(e) + u" res"_s;
}

double capOf(const LiveResists &resists, Element e)
{
    return resists.caps[e] != 0 ? resists.caps[e] : resists.cap;
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

} // namespace pou
