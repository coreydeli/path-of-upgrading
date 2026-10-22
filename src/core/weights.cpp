// SPDX-License-Identifier: MIT
#include "core/weights.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

const Weights &defaultWeights()
{
    static const Weights weights = {
        {u"# to maximum Life"_s, 1},
        {u"# to maximum Mana"_s, 0.3},
        {u"#% to Fire Resistance"_s, 1.5},
        {u"#% to Cold Resistance"_s, 1.5},
        {u"#% to Lightning Resistance"_s, 1.5},
        {u"#% to Chaos Resistance"_s, 1},
        {u"# to Strength"_s, 0.4},
        {u"# to Dexterity"_s, 0.4},
        {u"# to Intelligence"_s, 0.4},
        {u"#% increased Movement Speed"_s, 3},
        {u"[Base] Armour"_s, 0.05},
        {u"[Base] Evasion Rating"_s, 0.05},
        {u"[Base] Energy Shield"_s, 0.5},
        {u"[Base] Spirit"_s, 1},
        {u"[Weapon] Total DPS"_s, 1},
    };
    return weights;
}

} // namespace pou
