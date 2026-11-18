// SPDX-License-Identifier: MIT
#include "core/modtiers.h"

#include "core/parser.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// close enough for values that were rounded
constexpr double slack = 1e-6;

bool covers(const ModTier &t, double value)
{
    return value >= t.low - slack && value <= t.high + slack;
}

QList<ModTier> plainTiers(const SlotMods &mods, const QString &itemClass, const QString &key)
{
    QList<ModTier> plain = mods.tiers(itemClass, key);
    plain.removeIf([](const ModTier &t) { return t.hybrid; });
    return plain;
}

QString rangeText(const ModTier &t)
{
    return t.low == t.high ? rollText(t.high) : rollText(t.low) + u" to "_s + rollText(t.high);
}

} // namespace

std::optional<StatTier> tierOf(const SlotMods &mods, const QString &itemClass, const QString &key,
                               double value)
{
    const QList<ModTier> all = mods.tiers(itemClass, key);
    const QList<ModTier> plain = plainTiers(mods, itemClass, key);
    // a hybrid mod with the same stat could have given it as well
    if (plain.isEmpty() ||
        std::ranges::any_of(all, [value](const ModTier &t) { return t.hybrid && covers(t, value); }))
        return std::nullopt;
    qsizetype found = -1;
    for (qsizetype i = 0; i < plain.size(); ++i) {
        if (!covers(plain.at(i), value))
            continue;
        if (found >= 0)
            return std::nullopt;
        found = i;
    }
    if (found < 0)
        return std::nullopt;
    StatTier out{.number = int(found) + 1, .count = int(plain.size()), .range = plain.at(found)};
    if (found > 0) {
        out.better = plain.at(found - 1);
        out.betterNumber = int(found);
    }
    return out;
}

std::optional<StatTier> rolledTier(const SlotMods &mods, const Item &item, const QString &itemClass,
                                   const QString &key)
{
    int explicitMods = 0;
    bool others = false;
    double value = 0;
    for (const ItemLine &line : item.lines) {
        if (line.key.isEmpty() || line.tag == u"base"_s || line.tag == u"info"_s || line.tag == u"rune"_s ||
            line.tag == u"enchant"_s || line.tag == u"implicit"_s)
            continue;
        for (const auto &[k, v] : expandStat(line.key, line.value)) {
            if (k != key)
                continue;
            if (line.key == key && (line.tag == u"explicit"_s || line.tag == u"fractured"_s)) {
                ++explicitMods;
                value = v;
            } else {
                others = true;
            }
        }
    }
    if (explicitMods != 1 || others)
        return std::nullopt;
    return tierOf(mods, itemClass, key, value);
}

std::optional<int> tierFor(const SlotMods &mods, const QString &itemClass, const QString &key, double minimum)
{
    const QList<ModTier> plain = plainTiers(mods, itemClass, key);
    for (qsizetype i = plain.size(); i > 0; --i) {
        if (plain.at(i - 1).high + slack >= minimum)
            return int(i);
    }
    return std::nullopt;
}

QString tierText(const StatTier &tier)
{
    return u"tier %1 of %2"_s.arg(tier.number).arg(tier.count);
}

QString nextTierText(const StatTier &tier)
{
    if (!tier.better)
        return {};
    return u"tier %1 (%2) at item level %3 or higher"_s.arg(
        QString::number(tier.betterNumber), rangeText(*tier.better), QString::number(tier.better->level));
}

} // namespace pou
