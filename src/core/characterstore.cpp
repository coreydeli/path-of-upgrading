// SPDX-License-Identifier: MIT
#include "core/characterstore.h"

#include "core/parser.h"

#include <QJsonArray>
#include <QStringList>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

constexpr int formatVersion = 1;

QJsonObject numbersToJson(const QHash<QString, double> &numbers)
{
    QJsonObject o;
    for (auto it = numbers.cbegin(); it != numbers.cend(); ++it)
        o.insert(it.key(), it.value());
    return o;
}

QHash<QString, double> numbersFromJson(const QJsonObject &o)
{
    QHash<QString, double> numbers;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        if (it.value().isDouble())
            numbers.insert(it.key(), it.value().toDouble());
    }
    return numbers;
}

} // namespace

QJsonObject toJson(const Character &character)
{
    QJsonObject gear;
    for (auto it = character.gear.cbegin(); it != character.gear.cend(); ++it)
        gear.insert(it.key(), it.value().raw);

    QStringList fromPob(character.fromPob.cbegin(), character.fromPob.cend());
    std::sort(fromPob.begin(), fromPob.end());

    QJsonObject o = {
        {u"version"_s, formatVersion},
        {u"name"_s, character.name},
        {u"gear"_s, gear},
        {u"fromPob"_s, QJsonArray::fromStringList(fromPob)},
        {u"weights"_s, numbersToJson(character.weights)},
        {u"ignored"_s, toJson(character.ignored)},
        {u"buildProfile"_s, character.buildProfile},
        {u"tweaks"_s, numbersToJson(character.tweaks)},
        {u"chaosInoculation"_s, character.chaosInoculation},
    };
    if (character.resists)
        o.insert(u"resists"_s, toJson(*character.resists));
    return o;
}

std::optional<Character> characterFromJson(const QJsonObject &o)
{
    if (o.value(u"version").toInt() > formatVersion)
        return std::nullopt;

    Character c;
    c.name = o.value(u"name").toString();

    const QJsonObject gear = o.value(u"gear").toObject();
    for (auto it = gear.constBegin(); it != gear.constEnd(); ++it) {
        if (std::optional<Item> item = parseItem(it.value().toString()))
            c.gear.insert(it.key(), std::move(*item));
    }
    for (const QJsonValue &slot : o.value(u"fromPob").toArray()) {
        if (c.gear.contains(slot.toString()))
            c.fromPob.insert(slot.toString());
    }

    // no weights at all is a choice of yours, unlike weights never saved
    if (o.contains(u"weights"))
        c.weights = numbersFromJson(o.value(u"weights").toObject());
    c.ignored = ignoreRulesFromJson(o.value(u"ignored").toArray());
    c.buildProfile = o.value(u"buildProfile").toString();
    c.tweaks = numbersFromJson(o.value(u"tweaks").toObject());
    if (o.value(u"resists").isObject())
        c.resists = resistRecordFromJson(o.value(u"resists").toObject());
    c.chaosInoculation = o.value(u"chaosInoculation").toBool();
    return c;
}

} // namespace pou
