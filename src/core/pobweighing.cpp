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
    }
    return weighing;
}

} // namespace pou
