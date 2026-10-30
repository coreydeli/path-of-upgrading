// SPDX-License-Identifier: MIT
#include "core/buildimport.h"

#include "core/buildprofiles.h"
#include "core/parser.h"
#include "core/slots.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// The build's items by slot, the jewels in the order of their sockets.
QHash<QString, PobItemText> itemsOf(const PobBuild &build)
{
    QHash<QString, PobItemText> items = build.gear;
    const QStringList jewelSlots = slotGroup(u"Jewel"_s);
    for (qsizetype i = 0; i < build.jewels.size() && i < jewelSlots.size(); ++i)
        items.insert(jewelSlots.at(i), build.jewels.at(i));
    return items;
}

// What the build's numbers were worked out with.
Equipped gearOf(const QHash<QString, PobItemText> &items)
{
    Equipped gear;
    for (const QString &slot : allSlots()) {
        const auto text = items.constFind(slot);
        if (text == items.cend())
            continue;
        if (std::optional<Item> item = parseItem(text->text))
            gear.insert(slot, std::move(*item));
    }
    return gear;
}

std::optional<ResistRecord> resistsOf(const PobBuild &build, const Equipped &gear,
                                      const ResistRecord *previous)
{
    const bool any = std::any_of(build.resists.values.cbegin(), build.resists.values.cend(),
                                 [](const std::optional<double> &r) { return r.has_value(); });
    if (!any)
        return std::nullopt;
    return snapshotResists({.values = build.resists}, gear, ResistSource::Pob, previous);
}

} // namespace

BuildImport importBuild(const Character &character, const PobBuild &build)
{
    BuildImport out{.character = character};
    Character &c = out.character;
    for (const QString &slot : std::as_const(c.fromPob))
        c.gear.remove(slot);
    c.fromPob.clear();

    const QHash<QString, PobItemText> items = itemsOf(build);
    const Equipped theirs = gearOf(items);
    for (const QString &slot : allSlots()) {
        const auto item = theirs.constFind(slot);
        if (item == theirs.cend())
            continue;
        const auto worn = c.gear.constFind(slot);
        if (worn != c.gear.cend() && worn->name == item->name && worn->baseType == item->baseType) {
            ++out.kept;
            continue;
        }
        c.gear.insert(slot, *item);
        c.fromPob.insert(slot);
        ++out.items;
        if (items.value(slot).missingDamage)
            ++out.withoutDamage;
    }

    const bool fromSheet = c.resists && c.resists->source == ResistSource::Game;
    if (fromSheet)
        return out;
    if (std::optional<ResistRecord> resists = resistsOf(build, theirs, c.resists ? &*c.resists : nullptr)) {
        c.resists = std::move(resists);
        out.resists = true;
    }
    return out;
}

QString fittingProfile(const PobBuild &build)
{
    if (build.minionDps > 0)
        return minionProfile.toString();
    if (build.energyShield > build.life * 2)
        return energyShieldProfile.toString();
    return defaultBuildProfile.toString();
}

std::optional<ResistRecord> buildResists(const PobBuild &build, const ResistRecord *previous)
{
    return resistsOf(build, gearOf(itemsOf(build)), previous);
}

} // namespace pou
