// SPDX-License-Identifier: MIT
#include "core/catalog.h"

#include "core/statnorm.h"

#include <QCollator>
#include <QRegularExpression>
#include <algorithm>
#include <numeric>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// The pseudo stats that count an item's mods, and tablet uses.
bool isCount(const QString &text)
{
    static const QRegularExpression count(
        uR"(^# (Empty |Unrevealed |Desecrated |Fractured |Crafted |Enchant |Implicit |Prefix |Suffix )*)"
        uR"((Prefix |Suffix )?Modifiers$|uses remaining)"_s);
    return count.match(text).hasMatch();
}

// Lower is closer to how items word a stat: explicit mods read like the
// item, items never say "(Local)", and pseudo stats say "total".
int wordingRank(const TradeStat &stat)
{
    if (stat.type == u"pseudo")
        return 3;
    if (stat.type != u"explicit")
        return 2;
    return stat.local ? 1 : 0;
}

} // namespace

int CatalogEntry::seenOn(const QString &itemClass) const
{
    if (!itemClass.isEmpty())
        return seen.value(itemClass);
    return std::accumulate(seen.cbegin(), seen.cend(), 0);
}

StatCatalog buildCatalog(const StatIndex &stats, const SeenStats &seen)
{
    StatCatalog catalog;
    QHash<QString, qsizetype> byNormalized; // -> position in catalog.entries
    QList<int> wordings;                    // wordingRank() of each entry's text

    for (const TradeStat &stat : stats.all()) {
        if (stat.type == u"sanctum" || isCount(stat.text))
            continue;
        const QString normalized = normalizeStat(stat.text);
        qsizetype i = byNormalized.value(normalized, -1);
        if (i < 0) {
            i = catalog.entries.size();
            byNormalized.insert(normalized, i);
            catalog.entries.append({stat.text, normalized, {}, {}});
            wordings.append(wordingRank(stat));
        }
        CatalogEntry &entry = catalog.entries[i];
        if (!entry.types.contains(stat.type))
            entry.types.append(stat.type);
        if (const int rank = wordingRank(stat); rank < wordings.at(i)) {
            entry.text = stat.text;
            wordings[i] = rank;
        }
    }

    for (auto cls = seen.cbegin(); cls != seen.cend(); ++cls) {
        const QHash<QString, int> &counts = cls.value();
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            if (const qsizetype i = byNormalized.value(normalizeStat(it.key()), -1); i >= 0)
                catalog.entries[i].seen[cls.key()] += it.value();
        }
        catalog.classes << cls.key();
    }

    // the trade site's texts are English whatever language the app is in
    const QCollator collator{QLocale(QLocale::English)};
    const auto before = [&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    };
    std::stable_sort(
        catalog.entries.begin(), catalog.entries.end(),
        [&before](const CatalogEntry &a, const CatalogEntry &b) { return before(a.text, b.text); });
    std::sort(catalog.classes.begin(), catalog.classes.end(), before);
    return catalog;
}

QString statTypeLabel(const QString &type)
{
    if (type == u"pseudo")
        return u"Total"_s; // pseudo stats add up every version of a stat
    if (type == u"augment" || type == u"rune")
        return u"Rune"_s;
    if (type.isEmpty())
        return type;
    return type.front().toUpper() + type.sliced(1);
}

QString itemKeyFromText(const QString &text)
{
    static const QRegularExpression local(uR"(\s*\(Local\)\s*$)"_s,
                                          QRegularExpression::CaseInsensitiveOption);

    QString key = text;
    key.replace(u"+#"_s, u"#"_s);
    key.remove(local);
    return key.trimmed();
}

} // namespace pou
