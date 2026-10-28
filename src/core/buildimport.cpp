// SPDX-License-Identifier: MIT
#include "core/buildimport.h"

#include "core/parser.h"
#include "core/slots.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {

BuildImport importBuild(const Character &character, const PobBuild &build)
{
    BuildImport out{.character = character};
    Character &c = out.character;
    for (const QString &slot : std::as_const(c.fromPob))
        c.gear.remove(slot);
    c.fromPob.clear();

    QHash<QString, PobItemText> theirs = build.gear;
    const QStringList jewelSlots = slotGroup(u"Jewel"_s);
    for (qsizetype i = 0; i < build.jewels.size() && i < jewelSlots.size(); ++i)
        theirs.insert(jewelSlots.at(i), build.jewels.at(i));

    // what the build's numbers were worked out with
    Equipped buildGear;
    for (const QString &slot : allSlots()) {
        const auto text = theirs.constFind(slot);
        if (text == theirs.cend())
            continue;
        std::optional<Item> item = parseItem(text->text);
        if (!item)
            continue;
        buildGear.insert(slot, *item);

        const auto worn = c.gear.constFind(slot);
        if (worn != c.gear.cend() && worn->name == item->name && worn->baseType == item->baseType) {
            ++out.kept;
            continue;
        }
        c.gear.insert(slot, std::move(*item));
        c.fromPob.insert(slot);
        ++out.items;
        if (text->missingDamage)
            ++out.withoutDamage;
    }

    const bool anyResist = std::any_of(build.resists.values.cbegin(), build.resists.values.cend(),
                                       [](const std::optional<double> &r) { return r.has_value(); });
    const bool fromSheet = c.resists && c.resists->source == ResistSource::Game;
    if (anyResist && !fromSheet) {
        c.resists = snapshotResists({.values = build.resists}, buildGear, ResistSource::Pob,
                                    c.resists ? &*c.resists : nullptr);
        out.resists = true;
    }
    return out;
}

} // namespace pou
