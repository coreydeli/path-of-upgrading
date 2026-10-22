// SPDX-License-Identifier: MIT
#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace pou {

// A stat, or a preset group of stats, you don't want to see or search for.
struct IgnoreRule
{
    enum class Mode {
        Ignore,  // left out of scores, the popup and every search
        Exclude, // and trade searches skip listings that have it
    };

    QString preset; // id of one of ignorePresets(), or empty for a single stat
    QString key;    // the stat, when there's no preset
    Mode mode = Mode::Ignore;

    friend bool operator==(const IgnoreRule &, const IgnoreRule &) = default;
};

struct IgnorePreset
{
    QString id;
    QString label;
    QStringList keys;
};

const QList<IgnorePreset> &ignorePresets();

// The first rule that covers the stat, or null.
const IgnoreRule *ruleFor(const QList<IgnoreRule> &rules, const QString &key);
inline bool isIgnored(const QList<IgnoreRule> &rules, const QString &key)
{
    return ruleFor(rules, key) != nullptr;
}

// Stats a trade listing must not have.
QStringList excludedKeys(const QList<IgnoreRule> &rules);

// One line per rule for the popup: "All resistances (excluded)".
QStringList describe(const QList<IgnoreRule> &rules);

// Rules for unknown presets or without a stat are dropped.
QList<IgnoreRule> ignoreRulesFromJson(const QJsonArray &array);
QJsonArray toJson(const QList<IgnoreRule> &rules);

} // namespace pou
