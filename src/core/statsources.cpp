// SPDX-License-Identifier: MIT
#include "core/statsources.h"

#include "core/numbers.h"
#include "core/parser.h"

#include <QRegularExpression>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

StatOrigin originOf(const QString &tag)
{
    if (tag == u"rune" || tag == u"enchant")
        return StatOrigin::Socket;
    if (tag == u"implicit")
        return StatOrigin::Implicit;
    return StatOrigin::Mod;
}

bool isBaseKey(const QString &key)
{
    return key.startsWith(u'[');
}

void put(QList<StatSource> &out, const QString &key, double value, StatOrigin origin)
{
    const auto it = std::find_if(out.begin(), out.end(),
                                 [&](const StatSource &s) { return s.key == key && s.origin == origin; });
    if (it != out.end())
        it->value += value;
    else
        out.append({key, value, origin});
}

} // namespace

QList<StatSource> statSources(const Item &item)
{
    QList<StatSource> out;
    const bool hasStatLines =
        std::any_of(item.lines.begin(), item.lines.end(), [](const ItemLine &l) { return !l.key.isEmpty(); });
    if (!hasStatLines) {
        for (const auto &[key, value] : item.stats)
            out.append({key, value, isBaseKey(key) ? StatOrigin::Base : StatOrigin::Mod});
        return out;
    }

    for (const ItemLine &line : item.lines) {
        if (line.key.isEmpty() || line.tag == u"base" || line.tag == u"info")
            continue;
        const StatOrigin origin = originOf(line.tag);
        for (const auto &[key, value] : expandStat(line.key, line.value))
            put(out, key, value, origin);
    }
    for (const auto &[key, value] : item.stats) {
        if (isBaseKey(key))
            put(out, key, value, StatOrigin::Base);
    }
    for (StatSource &s : out)
        s.value = roundTo(s.value, 2);
    return out;
}

double baseWithoutSockets(const QString &key, double value, const QList<StatSource> &sources)
{
    static const QHash<QString, QString> words = {
        {u"[Base] Armour"_s, u"armour"_s},
        {u"[Base] Evasion Rating"_s, u"evasion"_s},
        {u"[Base] Energy Shield"_s, u"energy shield"_s},
        {u"[Base] Spirit"_s, u"spirit"_s},
    };
    // Global and minion versions don't scale the item's own base.
    static const QRegularExpression notLocal(uR"(maximum|Recharge|Rate|Allies|Minion)"_s,
                                             QRegularExpression::CaseInsensitiveOption);

    const QString word = words.value(key);
    if (word.isEmpty())
        return value;
    double mods = 0;
    double sockets = 0;
    for (const StatSource &s : sources) {
        if (s.origin == StatOrigin::Base || !s.key.startsWith(u"#% increased ") ||
            !s.key.contains(word, Qt::CaseInsensitive) || notLocal.match(s.key).hasMatch())
            continue;
        (s.origin == StatOrigin::Socket ? sockets : mods) += s.value;
    }
    if (sockets == 0)
        return value;
    return value * (100 + mods) / (100 + mods + sockets);
}

bool isInBase(const Item &item, const QString &key)
{
    static const QRegularExpression localDefence(
        uR"(^#% increased (Armour|Evasion Rating|Evasion|Energy Shield|Armour and Evasion|)"
        uR"(Armour and Energy Shield|Evasion and Energy Shield|Armour, Evasion and Energy Shield)$|)"
        uR"(^# to (maximum Energy Shield|Armour|Evasion Rating)$)"_s);

    const StatMap &s = item.stats;
    const bool hasBaseDefence = s.value(u"[Base] Armour") != 0 || s.value(u"[Base] Evasion Rating") != 0 ||
                                s.value(u"[Base] Energy Shield") != 0;
    if (hasBaseDefence && localDefence.match(key).hasMatch())
        return true;
    return s.value(u"[Base] Spirit") != 0 && key == u"#% increased Spirit";
}

} // namespace pou
