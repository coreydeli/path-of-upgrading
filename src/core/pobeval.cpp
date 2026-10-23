// SPDX-License-Identifier: MIT
#include "core/pobeval.h"

#include <QJsonArray>

using namespace Qt::Literals::StringLiterals;

namespace pou {

PobStats readPobStats(const QJsonObject &stats)
{
    PobStats out;
    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        if (it.value().isDouble())
            out.insert(it.key(), it.value().toDouble());
    }
    return out;
}

PobEval readPobEval(const QJsonObject &answer)
{
    PobEval eval;
    eval.before = readPobStats(answer.value(u"before").toObject());
    for (const QJsonValue result : answer.value(u"results").toArray()) {
        PobItemEval item;
        if (!result[u"ok"].toBool()) {
            item.error = result[u"error"].toString(u"Path of Building couldn't try this item."_s);
            eval.items << item;
            continue;
        }
        for (const QJsonValue placement : result[u"slots"].toArray()) {
            item.placements << PobPlacement{
                .slot = placement[u"slot"].toString(),
                .label = placement[u"label"].toString(),
                .replacing = placement[u"replacing"].toString(),
                .stats = readPobStats(placement[u"stats"].toObject()),
            };
        }
        eval.items << item;
    }
    return eval;
}

QString pobItemName(const Item &item)
{
    const QString rarity = item.rarity.toLower();
    if ((rarity == u"rare" || rarity == u"unique") && !item.baseType.isEmpty())
        return item.name + u", "_s + item.baseType;
    return item.name;
}

std::optional<PobJudgement> bestPlacement(const PobStats &before, const PobItemEval &eval, const Item &item,
                                          const PobVerdictOptions &options)
{
    const QString name = pobItemName(item);
    QList<PobPlacement> placements;
    for (const PobPlacement &placement : eval.placements) {
        if (!name.isEmpty() && placement.replacing == name)
            placements << placement;
    }
    if (placements.isEmpty())
        placements = eval.placements;

    QList<PobVerdict> verdicts;
    for (const PobPlacement &placement : placements)
        verdicts << judgeWithPob(before, placement.stats, options);
    const qsizetype best = bestVerdict(verdicts);
    if (best < 0)
        return std::nullopt;
    return PobJudgement{placements.at(best), verdicts.at(best)};
}

} // namespace pou
