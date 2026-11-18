// SPDX-License-Identifier: MIT
#include "core/slotmods.h"

#include "core/numbers.h"
#include "core/slots.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

using namespace Qt::Literals::StringLiterals;

namespace pou {

const SlotMods &SlotMods::builtIn()
{
    static const SlotMods table = [] {
        // a resource built into the program, not one of the user's files, so it
        // can be read here in core
        QFile file(u":/pou/slotmods.json"_s);
        if (!file.open(QIODevice::ReadOnly))
            return SlotMods();
        return fromJson(QJsonDocument::fromJson(file.readAll()).object());
    }();
    return table;
}

SlotMods SlotMods::fromJson(const QJsonObject &table)
{
    SlotMods mods;
    mods.m_version = table.value(u"version").toString();
    mods.m_date = table.value(u"date").toString();
    const QJsonObject classes = table.value(u"classes").toObject();
    for (auto cls = classes.constBegin(); cls != classes.constEnd(); ++cls) {
        const QJsonObject keys = cls.value().toObject();
        QHash<QString, QList<ModTier>> &into = mods.m_classes[cls.key().toLower()];
        for (auto key = keys.constBegin(); key != keys.constEnd(); ++key) {
            QList<ModTier> tiers;
            for (const QJsonValue &tier : key.value().toArray()) {
                const QJsonArray t = tier.toArray();
                if (t.size() == 3 || t.size() == 4)
                    tiers << ModTier{.level = t.at(0).toInt(),
                                     .low = t.at(1).toDouble(),
                                     .high = t.at(2).toDouble(),
                                     .hybrid = t.size() == 4};
            }
            if (!tiers.isEmpty())
                into.insert(key.key().toLower(), tiers);
        }
    }
    return mods;
}

QList<ModTier> SlotMods::tiers(const QString &itemClass, const QString &key) const
{
    return m_classes.value(itemClass.trimmed().toLower()).value(key.toLower());
}

bool SlotMods::allows(const QString &itemClass, const QString &key) const
{
    const auto cls = m_classes.constFind(itemClass.trimmed().toLower());
    return cls == m_classes.cend() || cls->contains(key.toLower());
}

namespace {

// A hybrid mod's ranges are for the other stat as much as this one, so they
// only count when the stat has no other.
QList<ModTier> plainOrAll(QList<ModTier> all)
{
    QList<ModTier> plain = all;
    plain.removeIf([](const ModTier &t) { return t.hybrid; });
    return plain.isEmpty() ? all : plain;
}

} // namespace

std::optional<ModTier> SlotMods::best(const QString &itemClass, const QString &key) const
{
    const QList<ModTier> all = plainOrAll(tiers(itemClass, key));
    if (all.isEmpty())
        return std::nullopt;
    // a tier with a lower item level can still roll higher, so look at all
    ModTier top = all.first();
    for (const ModTier &t : all) {
        if (t.high > top.high)
            top = t;
    }
    return top;
}

std::optional<ModTier> SlotMods::bestAt(const QString &itemClass, const QString &key, int itemLevel) const
{
    std::optional<ModTier> top;
    for (const ModTier &t : plainOrAll(tiers(itemClass, key))) {
        if (t.level <= itemLevel && (!top || t.high > top->high))
            top = t;
    }
    return top;
}

QString classOfSlot(const QString &slot)
{
    static const QHash<QString, QString> classes = {
        {u"Helmet"_s, u"Helmets"_s}, {u"Body Armour"_s, u"Body Armours"_s},
        {u"Gloves"_s, u"Gloves"_s},  {u"Boots"_s, u"Boots"_s},
        {u"Belt"_s, u"Belts"_s},     {u"Amulet"_s, u"Amulets"_s},
        {u"Ring"_s, u"Rings"_s},
    };
    return classes.value(groupOf(slot));
}

QString rollText(double value)
{
    return formatNumber(roundTo(value, 2));
}

} // namespace pou
