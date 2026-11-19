// SPDX-License-Identifier: MIT
#include "core/wornmods.h"

#include <QRegularExpression>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// A mod as the popup would say it: advanced copies add the roll range,
// "+33(33-41) to maximum Life", and a marker after it, "(fractured)".
QString plainText(const QString &text)
{
    static const QRegularExpression range(uR"(\((-?\d+(?:\.\d+)?)[-–](-?\d+(?:\.\d+)?)\))"_s);
    static const QRegularExpression unscalable(uR"(\s+[—–-]+\s*Unscalable Value\s*$)"_s,
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tag(
        uR"(\s*\((implicit|rune|enchant|crafted|fractured|desecrated|augmented|unmet|veiled|mutated)\)\s*$)"_s,
        QRegularExpression::CaseInsensitiveOption);
    QString clean = QString(text).remove(range).remove(unscalable);
    // an advanced copy can show two of them
    while (clean.contains(tag))
        clean.remove(tag);
    return clean.trimmed();
}

QString groupOf(const Item &item, const ItemLine &line)
{
    if (line.tag == u"implicit")
        return u"Implicit"_s;
    if (line.tag == u"rune")
        return u"Runes"_s;
    if (line.tag == u"enchant")
        return u"Enchants"_s;
    if (item.rarity == u"Unique")
        return u"Mods"_s;
    switch (line.affix) {
    case AffixKind::Prefix:
        return u"Prefixes"_s;
    case AffixKind::Suffix:
        return u"Suffixes"_s;
    case AffixKind::Unknown:
        break;
    }
    return u"Other mods"_s;
}

} // namespace

QList<WornMod> wornMods(const Item &item)
{
    QList<WornMod> mods;
    for (const ItemLine &line : item.lines) {
        // base and other properties aren't mods
        if (line.tag == u"base" || line.tag == u"info")
            continue;
        mods << WornMod{.group = groupOf(item, line),
                        .text = plainText(line.text),
                        .fractured = line.tag == u"fractured"};
    }
    static const QStringList order = {u"Implicit"_s, u"Prefixes"_s, u"Suffixes"_s,  u"Runes"_s,
                                      u"Enchants"_s, u"Mods"_s,     u"Other mods"_s};
    std::ranges::stable_sort(mods, [](const WornMod &a, const WornMod &b) {
        return order.indexOf(a.group) < order.indexOf(b.group);
    });
    return mods;
}

bool hasFracturedMod(const Item &item)
{
    return std::ranges::any_of(item.lines, [](const ItemLine &line) { return line.tag == u"fractured"; });
}

} // namespace pou
