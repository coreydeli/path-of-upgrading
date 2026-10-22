// SPDX-License-Identifier: MIT
#include "core/character.h"

#include "core/numbers.h"
#include "core/slots.h"

#include <QHash>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

using Gear = QHash<QString, const Item *>;

struct Tracked
{
    QString label;
    QStringList keys;
    // a stat that only adds up in some slots
    QString extraKey;
    QStringList extraSlots;
};

const QList<Tracked> &tracked()
{
    static const QList<Tracked> list = {
        {u"Minion skill levels"_s, {u"# to Level of all Minion Skills"_s}, {}, {}},
        {u"Spirit"_s, {u"[Base] Spirit"_s, u"# to Spirit"_s}, {}, {}},
        // flat ES on armour is already in its Energy Shield; on jewellery it adds to yours
        {u"Energy Shield"_s,
         {u"[Base] Energy Shield"_s},
         u"# to maximum Energy Shield"_s,
         {u"Amulet"_s, u"Ring 1"_s, u"Ring 2"_s, u"Belt"_s}},
        {u"Life"_s, {u"# to maximum Life"_s}, {}, {}},
        {u"Mana"_s, {u"# to maximum Mana"_s}, {}, {}},
        {u"Intelligence"_s, {u"# to Intelligence"_s}, {}, {}},
        {u"Strength"_s, {u"# to Strength"_s}, {}, {}},
        {u"Dexterity"_s, {u"# to Dexterity"_s}, {}, {}},
        {u"Movement speed"_s, {u"#% increased Movement Speed"_s}, {}, {}},
    };
    return list;
}

double statOf(const Item *item, const QString &key)
{
    return item ? item->stats.value(key) : 0;
}

double totalFor(const Gear &gear, const Tracked &t)
{
    double total = 0;
    for (auto it = gear.cbegin(); it != gear.cend(); ++it) {
        if (!mainSlots().contains(it.key()))
            continue;
        for (const QString &key : t.keys)
            total += statOf(it.value(), key);
        if (t.extraSlots.contains(it.key()))
            total += statOf(it.value(), t.extraKey);
    }
    return total;
}

std::optional<double> averageScore(const Gear &gear, const GearScoreFn &gearScore)
{
    double sum = 0;
    int count = 0;
    for (const QString &slot : mainSlots()) {
        const Item *item = gear.value(slot);
        if (!item)
            continue;
        if (const std::optional<double> s = gearScore(*item, slot)) {
            sum += *s;
            ++count;
        }
    }
    if (count == 0)
        return std::nullopt;
    return roundHalfUp(sum / count);
}

} // namespace

std::optional<CharacterChange> changeIfEquipped(const Equipped &equipped, const Item &item,
                                                const QStringList &targets, const LiveResists *resists,
                                                const GearScoreFn &gearScore)
{
    if (targets.isEmpty())
        return std::nullopt;

    Gear before;
    for (auto it = equipped.cbegin(); it != equipped.cend(); ++it)
        before.insert(it.key(), &it.value());
    Gear after = before;
    for (const QString &slot : targets)
        after.remove(slot);
    after.insert(targets.first(), &item);

    CharacterChange change;
    change.targets = targets;

    for (const Tracked &t : tracked()) {
        const double a = totalFor(before, t);
        const double b = totalFor(after, t);
        if (const double delta = roundTo(b - a, 1); delta != 0)
            change.totals << TotalChange{t.label, roundTo(a, 1), roundTo(b, 1), delta};
    }

    const double cap = resists && resists->cap != 0 ? resists->cap : 75;
    const double buffer = resists ? resists->buffer : 0;
    for (Element e : allElements) {
        const QString key = resistKey(e);
        double removed = 0;
        for (const QString &slot : targets)
            removed += statOf(before.value(slot), key);
        const double delta = item.stats.value(key) - removed;
        if (delta == 0)
            continue;

        ResistChange r;
        r.element = e;
        r.label = elementName(e) + u" res"_s;
        r.delta = delta;
        r.cap = resists && resists->caps[e] != 0 ? resists->caps[e] : cap;
        r.safe = r.cap + buffer;
        if (resists && resists->values[e]) {
            const double from = *resists->values[e];
            const double to = from + delta;
            r.from = from;
            r.to = to;
            r.under = to < r.cap;
            r.wasUnder = from < r.cap;
            r.thin = to >= r.cap && to < r.safe;
            r.wasThin = from >= r.cap && from < r.safe;
        }
        change.resists << r;
    }

    if (gearScore) {
        const std::optional<double> a = averageScore(before, gearScore);
        const std::optional<double> b = averageScore(after, gearScore);
        if (a && b)
            change.overall = TotalChange{u"Gear score"_s, *a, *b, *b - *a};
    }

    const QString source =
        resists && resists->source == ResistSource::Pob ? u"Path of Building's"_s : u"Your"_s;
    for (const ResistChange &r : std::as_const(change.resists)) {
        if (!r.from || r.delta >= 0)
            continue;
        if (r.under && !r.wasUnder) {
            change.warnings << u"%1 drops below the cap (%2% of %3%)"_s.arg(r.label, formatNumber(*r.to),
                                                                            formatNumber(r.cap));
        } else if (r.under) {
            change.warnings << u"%1 goes further under the cap (from %2% to %3%)"_s.arg(
                r.label, formatNumber(*r.from), formatNumber(*r.to));
        } else if (r.thin && *r.from >= r.safe) {
            change.warnings << u"%1 would be only %2% over the cap (%3%). %4 numbers can include charms and "
                               "buffs that aren't always on, so check your character sheet."_s.arg(
                                   r.label, formatNumber(*r.to - r.cap), formatNumber(r.delta), source);
        }
    }
    return change;
}

} // namespace pou
