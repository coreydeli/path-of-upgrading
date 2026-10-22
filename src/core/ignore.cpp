// SPDX-License-Identifier: MIT
#include "core/ignore.h"

#include "core/statnorm.h"

#include <QJsonObject>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

const IgnorePreset *findPreset(const QString &id)
{
    for (const IgnorePreset &p : ignorePresets()) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

} // namespace

const QList<IgnorePreset> &ignorePresets()
{
    static const QList<IgnorePreset> presets = {
        {u"res"_s,
         u"All resistances"_s,
         {u"#% to Fire Resistance"_s, u"#% to Cold Resistance"_s, u"#% to Lightning Resistance"_s,
          u"#% to Chaos Resistance"_s}},
        {u"eleres"_s,
         u"Elemental resistances"_s,
         {u"#% to Fire Resistance"_s, u"#% to Cold Resistance"_s, u"#% to Lightning Resistance"_s}},
        {u"chaosres"_s, u"Chaos resistance"_s, {u"#% to Chaos Resistance"_s}},
        {u"attributes"_s, u"Attributes"_s, {u"# to Strength"_s, u"# to Dexterity"_s, u"# to Intelligence"_s}},
        {u"life"_s, u"Maximum Life"_s, {u"# to maximum Life"_s}},
        {u"mana"_s, u"Maximum Mana"_s, {u"# to maximum Mana"_s}},
    };
    return presets;
}

const IgnoreRule *ruleFor(const QList<IgnoreRule> &rules, const QString &key)
{
    const QString normalized = normalizeStat(key);
    for (const IgnoreRule &rule : rules) {
        if (!rule.preset.isEmpty()) {
            if (const IgnorePreset *p = findPreset(rule.preset); p && p->keys.contains(key))
                return &rule;
        } else if (rule.key == key || normalizeStat(rule.key) == normalized) {
            return &rule;
        }
    }
    return nullptr;
}

QStringList excludedKeys(const QList<IgnoreRule> &rules)
{
    QStringList out;
    const auto append = [&out](const QString &key) {
        if (!out.contains(key))
            out << key;
    };
    for (const IgnoreRule &rule : rules) {
        if (rule.mode != IgnoreRule::Mode::Exclude)
            continue;
        if (rule.preset.isEmpty()) {
            append(rule.key);
        } else if (const IgnorePreset *p = findPreset(rule.preset)) {
            for (const QString &key : p->keys)
                append(key);
        }
    }
    return out;
}

QStringList describe(const QList<IgnoreRule> &rules)
{
    QStringList out;
    for (const IgnoreRule &rule : rules) {
        const IgnorePreset *p = findPreset(rule.preset);
        QString line = p ? p->label : rule.key;
        if (rule.mode == IgnoreRule::Mode::Exclude)
            line += u" (excluded)"_s;
        out << line;
    }
    return out;
}

QList<IgnoreRule> ignoreRulesFromJson(const QJsonArray &array)
{
    QList<IgnoreRule> rules;
    for (const QJsonValue &v : array) {
        const QJsonObject o = v.toObject();
        IgnoreRule rule;
        rule.preset = o.value(u"preset").toString();
        rule.key = rule.preset.isEmpty() ? o.value(u"key").toString() : QString();
        rule.mode =
            o.value(u"mode").toString() == u"exclude" ? IgnoreRule::Mode::Exclude : IgnoreRule::Mode::Ignore;
        if (rule.preset.isEmpty() ? rule.key.isEmpty() : !findPreset(rule.preset))
            continue;
        rules << rule;
    }
    return rules;
}

QJsonArray toJson(const QList<IgnoreRule> &rules)
{
    QJsonArray array;
    for (const IgnoreRule &rule : rules) {
        QJsonObject o;
        if (rule.preset.isEmpty())
            o.insert(u"key", rule.key);
        else
            o.insert(u"preset", rule.preset);
        o.insert(u"mode", rule.mode == IgnoreRule::Mode::Exclude ? u"exclude"_s : u"ignore"_s);
        array << o;
    }
    return array;
}

} // namespace pou
