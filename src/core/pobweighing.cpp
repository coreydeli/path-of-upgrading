// SPDX-License-Identifier: MIT
#include "core/pobweighing.h"

#include <QJsonArray>
#include <algorithm>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {

QList<PobStatWeight> pobSearchStats(const PobStats &build, double damageWeight)
{
    const double damage = std::clamp(damageWeight, 0.0, 1.0);
    // which it calls by where it keeps them
    QString stat = pobDamageStat(build);
    if (stat == u"MinionDPS")
        stat = u"MinionCombinedDPS"_s;
    return {{stat, damage}, {u"TotalEHP"_s, std::max(0.05, 1 - damage)}};
}

SlotWeighing slotWeighing(const QString &group, const PobWeighing &weighing)
{
    SlotWeighing out{.group = group, .mods = {}};
    for (const ModWeight &mod : weighing.mods) {
        const PobModAbout about = weighing.about.value(mod.id);
        const QString key = statKeyOf(about.text);
        if (!key.isEmpty())
            out.mods << WeighedMod{.key = key, .weight = mod.weight, .top = about.top};
    }
    return out;
}

PobWeighing readPobWeighing(const QJsonObject &answer)
{
    PobWeighing weighing;
    weighing.category = answer.value(u"category").toString();
    weighing.min = std::max(0.0, answer.value(u"min").toDouble());
    for (const QJsonValue mod : answer.value(u"mods").toArray()) {
        const QString id = mod[u"id"].toString();
        const double weight = mod[u"weight"].toDouble();
        if (!id.isEmpty() && std::isfinite(weight) && weight > 0)
            weighing.mods << ModWeight{.id = id, .weight = weight};
        else
            continue;
        const double top = mod[u"top"].toDouble();
        weighing.about.insert(
            id, {.text = mod[u"text"].toString(), .top = std::isfinite(top) ? std::max(0.0, top) : 0});
    }
    return weighing;
}

} // namespace pou
