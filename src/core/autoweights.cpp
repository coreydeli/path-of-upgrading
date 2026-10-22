// SPDX-License-Identifier: MIT
#include "core/autoweights.h"

#include "core/numbers.h"
#include "core/statsources.h"

#include <QCollator>
#include <QHash>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <numeric>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Weapon and shield properties that only matter as part of the weapon's DPS,
// or that come with the base type.
bool isSkipped(const QString &key)
{
    static const QStringList keys = {
        u"[Weapon] Physical DPS"_s,       u"[Weapon] Elemental DPS"_s,       u"[Weapon] Chaos DPS"_s,
        u"[Weapon] Attacks per Second"_s, u"[Weapon] Critical Hit Chance"_s, u"[Base] Block Chance"_s,
        u"[Base] Runic Ward"_s,
    };
    return keys.contains(key);
}

// What a typical roll should be worth, in points.
double targetPoints(const QString &key, int items)
{
    if (key == u"#% increased Movement Speed")
        return 90; // what boots are for
    if (key == u"[Weapon] Total DPS")
        return 60;
    if (key.startsWith(u"[Base]"))
        return 40;
    // the more of your items have it, the more your build wants it
    return std::min(30 * (1 + 0.5 * (items - 1)), 90.0);
}

// 0.13, 1.5, 15: a suggestion needs no more
double toTwoDigits(double v)
{
    return roundTo(v, 1 - int(std::floor(std::log10(v))));
}

} // namespace

QList<SuggestedWeight> suggestWeights(const Equipped &equipped, const StatFilter &ignore)
{
    // stat key -> what each item that has it rolled
    QHash<QString, QList<double>> rolls;
    for (const Item &item : equipped) {
        if (item.rarity.contains(u"unique", Qt::CaseInsensitive))
            continue;
        QHash<QString, double> onItem;
        for (const StatSource &s : statSources(item)) {
            if (s.origin == StatOrigin::Socket || !(s.value > 0) || isSkipped(s.key) || isInBase(item, s.key))
                continue;
            if (ignore && ignore(s.key))
                continue;
            onItem[s.key] += s.value;
        }
        for (auto it = onItem.cbegin(); it != onItem.cend(); ++it)
            rolls[it.key()] << it.value();
    }

    QList<SuggestedWeight> out;
    for (auto it = rolls.cbegin(); it != rolls.cend(); ++it) {
        const QList<double> &values = it.value();
        const int items = int(values.size());
        const double average = std::accumulate(values.cbegin(), values.cend(), 0.0) / items;
        out.append(
            {it.key(), toTwoDigits(targetPoints(it.key(), items) / average), items, roundTo(average, 1)});
    }

    // stat texts are English whatever language the app is in
    const QCollator collator{QLocale(QLocale::English)};
    std::sort(out.begin(), out.end(), [&collator](const SuggestedWeight &a, const SuggestedWeight &b) {
        if (a.items != b.items)
            return a.items > b.items;
        return collator.compare(a.key, b.key) < 0;
    });
    return out;
}

} // namespace pou
