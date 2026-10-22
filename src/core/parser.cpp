// SPDX-License-Identifier: MIT
#include "core/parser.h"

#include "core/slots.h"

#include <QHash>
#include <QRegularExpression>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

constexpr auto CaseInsensitive = QRegularExpression::CaseInsensitiveOption;

const QRegularExpression &numberPattern()
{
    static const QRegularExpression re(uR"([+-]?\d+(?:\.\d+)?)"_s);
    return re;
}

// The trailing "(implicit)", "(rune)"... marker of a mod line.
const QRegularExpression &tagPattern()
{
    static const QRegularExpression re(
        uR"(\s*\((implicit|rune|enchant|crafted|fractured|desecrated|augmented|unmet|veiled|mutated)\)\s*$)"_s,
        CaseInsensitive);
    return re;
}

QList<double> numbersIn(const QString &text)
{
    QList<double> out;
    for (const QRegularExpressionMatch &m : numberPattern().globalMatch(text))
        out << m.captured().toDouble();
    return out;
}

QString capitalized(const QString &word)
{
    return word.left(1).toUpper() + word.mid(1).toLower();
}

// Base properties shown at the top of armour, weapons and shields.
QString propertyKey(const QString &name)
{
    static const QHash<QString, QString> keys = {
        {u"armour"_s, u"[Base] Armour"_s},
        {u"evasion rating"_s, u"[Base] Evasion Rating"_s},
        {u"energy shield"_s, u"[Base] Energy Shield"_s},
        {u"block chance"_s, u"[Base] Block Chance"_s},
        {u"chance to block"_s, u"[Base] Block Chance"_s},
        {u"spirit"_s, u"[Base] Spirit"_s},
        {u"runic ward"_s, u"[Base] Runic Ward"_s},
    };
    return keys.value(name);
}

bool skipLine(const QString &line)
{
    static const QRegularExpression re(
        uR"(^(?:Requirements:?$|Requires\b|Level:|Str:|Dex:|Int:|Item Level:)"
        uR"(|Sockets:|Quality\b|Corrupted$|Unidentified$|Mirrored$|Note:|Rarity:)"
        uR"(|Item Class:|Stack Size:|Waystone Tier:|Unmodifiable$|Sanctified$)"
        uR"(|Twice Corrupted$))"_s,
        CaseInsensitive);
    return re.match(line).hasMatch();
}

// "+32% to Cold Resistance" -> {"#% to Cold Resistance", 32}
std::optional<StatMap::Entry> modToStat(const QString &text)
{
    static const QRegularExpression spaces(uR"(\s+)"_s);
    static const QRegularExpression reduced(uR"(\breduced\b)"_s, CaseInsensitive);
    static const QRegularExpression less(uR"(\bless\b)"_s, CaseInsensitive);

    const QList<double> values = numbersIn(text);
    if (values.isEmpty())
        return std::nullopt;
    QString key = QString(text).replace(numberPattern(), u"#"_s).replace(spaces, u" "_s).trimmed();

    // "Adds 10 to 20 Fire Damage" counts as the average of the range
    double value = values.size() == 2 && key.contains(u"# to #") ? (values[0] + values[1]) / 2 : values[0];

    // opposite wordings are folded into one key so they net against each other
    if (const auto m = reduced.match(key); m.hasMatch()) {
        key.replace(m.capturedStart(), m.capturedLength(), u"increased"_s);
        value = -std::abs(value);
    }
    if (const auto m = less.match(key); m.hasMatch()) {
        key.replace(m.capturedStart(), m.capturedLength(), u"more"_s);
        value = -std::abs(value);
    }
    return StatMap::Entry{key, value};
}

QList<QStringList> splitBlocks(const QString &text)
{
    static const QRegularExpression separator(uR"(^-{4,}\s*$)"_s);
    QList<QStringList> blocks;
    QStringList current;
    for (const QString &line : text.split(u'\n')) {
        if (separator.match(line).hasMatch()) {
            blocks << current;
            current.clear();
        } else if (const QString trimmed = line.trimmed(); !trimmed.isEmpty()) {
            current << trimmed;
        }
    }
    blocks << current;
    return blocks;
}

} // namespace

QList<StatMap::Entry> expandStat(const QString &key, double value)
{
    static const QRegularExpression allElemental(uR"(^#% to all Elemental Resistances$)"_s, CaseInsensitive);
    static const QRegularExpression twoResists(
        uR"(^#% to (Fire|Cold|Lightning|Chaos) and (Fire|Cold|Lightning|Chaos) Resistances$)"_s,
        CaseInsensitive);
    static const QRegularExpression allAttributes(uR"(^# to all Attributes$)"_s, CaseInsensitive);
    static const QRegularExpression twoAttributes(
        uR"(^# to (Strength|Dexterity|Intelligence) and (Strength|Dexterity|Intelligence)$)"_s,
        CaseInsensitive);

    if (allElemental.match(key).hasMatch()) {
        return {{u"#% to Fire Resistance"_s, value},
                {u"#% to Cold Resistance"_s, value},
                {u"#% to Lightning Resistance"_s, value}};
    }
    if (const auto m = twoResists.match(key); m.hasMatch()) {
        return {{u"#% to "_s + capitalized(m.captured(1)) + u" Resistance"_s, value},
                {u"#% to "_s + capitalized(m.captured(2)) + u" Resistance"_s, value}};
    }
    if (allAttributes.match(key).hasMatch())
        return {{u"# to Strength"_s, value}, {u"# to Dexterity"_s, value}, {u"# to Intelligence"_s, value}};
    if (const auto m = twoAttributes.match(key); m.hasMatch()) {
        return {{u"# to "_s + capitalized(m.captured(1)), value},
                {u"# to "_s + capitalized(m.captured(2)), value}};
    }
    return {{key, value}};
}

std::optional<Item> parseItem(const QString &input)
{
    const QString text = QString(input).replace(u"\r\n"_s, u"\n"_s).replace(u'\r', u'\n').trimmed();
    if (!text.startsWith(u"Item Class:", Qt::CaseInsensitive))
        return std::nullopt;

    static const QRegularExpression classLine(uR"(^Item Class:\s*(.+)$)"_s, CaseInsensitive);
    static const QRegularExpression rarityLine(uR"(^Rarity:\s*(.+)$)"_s, CaseInsensitive);
    static const QRegularExpression itemLevelLine(uR"(^Item Level:\s*(\d+))"_s, CaseInsensitive);
    static const QRegularExpression levelLine(uR"(^Level:\s*(\d+))"_s, CaseInsensitive);
    static const QRegularExpression requiresLine(uR"(^Requires:?\s*Level\s*(\d+))"_s, CaseInsensitive);
    static const QRegularExpression corruptedLine(uR"(^Corrupted$)"_s, CaseInsensitive);
    static const QRegularExpression unidentifiedLine(uR"(^Unidentified$)"_s, CaseInsensitive);
    static const QRegularExpression propertyLine(uR"(^([A-Za-z ]+):\s*(.+)$)"_s);

    const QList<QStringList> blocks = splitBlocks(text);

    Item item;
    item.raw = text;

    QStringList names;
    for (const QString &line : blocks.first()) {
        if (const auto m = classLine.match(line); m.hasMatch())
            item.itemClass = m.captured(1).trimmed();
        else if (const auto r = rarityLine.match(line); r.hasMatch())
            item.rarity = r.captured(1).trimmed();
        else
            names << line;
    }
    item.name = names.value(0);
    item.baseType = names.value(1, names.value(0));
    // A magic item's only name line includes its affixes ("Hale Gold Ring of
    // the Whelpling") and normal items can be "Superior ...": neither is a base
    // type the trade site knows.
    const QString rarity = item.rarity.toLower();
    if (names.size() < 2 && rarity == u"magic")
        item.baseType.clear();
    if (rarity == u"normal" && item.baseType.startsWith(u"Superior "))
        item.baseType = item.baseType.mid(9);
    item.slot = slotForClass(item.itemClass);
    item.twoHanded = isTwoHanded(item.itemClass);

    const auto add = [&item](const QString &key, double value) {
        for (const auto &[k, v] : expandStat(key, value))
            item.stats.add(k, v);
    };

    for (qsizetype b = 1; b < blocks.size(); ++b) {
        const QStringList &block = blocks.at(b);
        for (const QString &line : block) {
            const QString &clean = line;

            if (const auto m = itemLevelLine.match(clean); m.hasMatch()) {
                item.itemLevel = m.captured(1).toInt();
                continue;
            }
            if (const auto m = levelLine.match(clean); m.hasMatch()) {
                item.requiredLevel = m.captured(1).toInt();
                continue;
            }
            if (const auto m = requiresLine.match(clean); m.hasMatch()) {
                item.requiredLevel = m.captured(1).toInt();
                continue;
            }
            if (corruptedLine.match(clean).hasMatch()) {
                item.corrupted = true;
                continue;
            }
            if (unidentifiedLine.match(clean).hasMatch()) {
                item.unidentified = true;
                continue;
            }
            if (skipLine(clean))
                continue;

            if (const auto prop = propertyLine.match(clean); prop.hasMatch()) {
                const QString name = prop.captured(1).trimmed().toLower();
                const QString value = prop.captured(2).replace(tagPattern(), QString());
                if (const QString key = propertyKey(name); !key.isEmpty()) {
                    if (const QList<double> n = numbersIn(value); !n.isEmpty()) {
                        add(key, n.first());
                        item.lines << ItemLine{line, key, n.first(), u"base"_s};
                    }
                    continue;
                }
                // Other "X: Y" lines (Grants Skill, Reload Time...) are shown but not scored.
                item.lines << ItemLine{line, {}, 0, u"info"_s};
                continue;
            }

            const auto tagMatch = tagPattern().match(clean);
            const QString tag = tagMatch.hasMatch() ? tagMatch.captured(1).toLower() : u"explicit"_s;
            const QString modText =
                QString(clean).replace(tagPattern(), QString()).replace(tagPattern(), QString()).trimmed();

            const auto stat = modToStat(modText);
            if (!stat) {
                // flavour text and keyword mods without numbers
                if (block.size() <= 6 && item.rarity != u"Unique")
                    item.lines << ItemLine{line, {}, 0, tag};
                continue;
            }
            add(stat->first, stat->second);
            item.lines << ItemLine{line, stat->first, stat->second, tag};
        }
    }

    return item;
}

} // namespace pou
