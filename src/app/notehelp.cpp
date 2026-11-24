// SPDX-License-Identifier: MIT
#include "app/notehelp.h"

#include <array>
#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString noteHelp(QStringView label)
{
    static const std::array<std::pair<QString, QString>, 9> help{{
        {u"Missing stats"_s,
         u"Stats your build counts on that this item doesn't have, with the most its type "
         u"can roll of each."_s},
        {u"Not counted"_s, u"Mods on this item your gear score doesn't rate for your build, so they add "
                           u"nothing to its score."_s},
        {u"Weak rolls"_s, u"Stats this item has that rolled low next to the most its type can roll."_s},
        {u"Next tier"_s, u"The next tier up of a mod this item has, and the item level an item needs to "
                         u"roll it."_s},
        {u"Look for"_s, u"What to search for in an upgrade: the stats your build wants most here, each with "
                        u"the lowest roll worth having."_s},
        {u"Can roll"_s, u"The best each of those stats can roll on this type of item, and the item level it "
                        u"needs."_s},
        {u"Your build wants"_s, u"Nothing is worn here. These are the stats your build wants most in this "
                                u"slot, with the most each can roll."_s},
        {u"Unique"_s, u"A unique's mods are fixed, so your gear score doesn't rate it."_s},
        {u"Judged by"_s, u"A unique is judged by what it does for your damage and defences in Path of "
                         u"Building, once a build is loaded. Without one it gets no verdict."_s},
    }};
    for (const auto &[name, text] : help) {
        if (name == label)
            return text;
    }
    return {};
}

} // namespace pou
