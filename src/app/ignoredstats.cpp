// SPDX-License-Identifier: MIT
#include "app/ignoredstats.h"

#include "app/activecharacter.h"
#include "core/statnorm.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

IgnoredStats::Mode modeOf(const IgnoreRule &rule)
{
    return rule.mode == IgnoreRule::Mode::Exclude ? IgnoredStats::Excluded : IgnoredStats::Ignored;
}

IgnoreRule::Mode ruleMode(int mode)
{
    return mode == IgnoredStats::Excluded ? IgnoreRule::Mode::Exclude : IgnoreRule::Mode::Ignore;
}

// A rule for the stat by itself rather than in a group.
bool isFor(const IgnoreRule &rule, const QString &key)
{
    return rule.preset.isEmpty() && rule.key == key;
}

const IgnoreRule *singleRule(const QList<IgnoreRule> &rules, const QString &key)
{
    const auto it = std::ranges::find_if(rules, [&key](const IgnoreRule &rule) { return isFor(rule, key); });
    return it != rules.cend() ? &*it : nullptr;
}

const IgnoreRule *groupRule(const QList<IgnoreRule> &rules, const QString &id)
{
    const auto it = std::ranges::find_if(rules, [&id](const IgnoreRule &rule) { return rule.preset == id; });
    return it != rules.cend() ? &*it : nullptr;
}

} // namespace

IgnoredStats::IgnoredStats(ActiveCharacter *active, QObject *parent)
    : QAbstractListModel(parent)
    , m_active(active)
{
    connect(active, &ActiveCharacter::changed, this, &IgnoredStats::refresh);
    refresh();
}

int IgnoredStats::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_keys.size());
}

QVariant IgnoredStats::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const QString &key = m_keys.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return statName(key);
    case KeyRole:
        return key;
    case NoteRole:
        return propertyNote(key);
    case ModeRole:
        if (const IgnoreRule *rule = singleRule(m_active->character().ignored, key))
            return int(modeOf(*rule));
        return int(Counts);
    }
    return {};
}

QHash<int, QByteArray> IgnoredStats::roleNames() const
{
    return {{KeyRole, "key"}, {NameRole, "name"}, {NoteRole, "note"}, {ModeRole, "mode"}};
}

QString IgnoredStats::character() const
{
    return m_active->character().name;
}

QStringList IgnoredStats::modes() const
{
    return {u"Counts"_s, u"Ignored"_s, u"Excluded"_s};
}

QStringList IgnoredStats::keys() const
{
    QStringList keys = m_keys;
    for (const IgnoredGroup &group : m_groups) {
        if (group.mode == Counts)
            continue;
        for (const IgnorePreset &preset : ignorePresets()) {
            if (preset.id == group.id)
                keys << preset.keys;
        }
    }
    keys.removeDuplicates();
    return keys;
}

void IgnoredStats::setGroupMode(const QString &id, int mode)
{
    const auto known = std::ranges::find(m_groups, id, &IgnoredGroup::id);
    if (known == m_groups.end() || mode < Counts || mode > Excluded || known->mode == mode)
        return;
    change([&](QList<IgnoreRule> &rules) {
        const auto it = std::ranges::find(rules, id, &IgnoreRule::preset);
        if (mode == Counts)
            rules.removeIf([&id](const IgnoreRule &rule) { return rule.preset == id; });
        else if (it != rules.end())
            it->mode = ruleMode(mode);
        else
            rules << IgnoreRule{.preset = id, .key = {}, .mode = ruleMode(mode)};
    });
}

void IgnoredStats::add(const QString &key)
{
    if (key.isEmpty() || m_keys.contains(key) || !m_active->exists())
        return;
    change([&key](QList<IgnoreRule> &rules) {
        rules << IgnoreRule{.preset = {}, .key = key, .mode = IgnoreRule::Mode::Ignore};
    });
}

void IgnoredStats::setMode(const QString &key, int mode)
{
    const IgnoreRule *rule = singleRule(m_active->character().ignored, key);
    if (!rule || (mode != Ignored && mode != Excluded) || modeOf(*rule) == mode)
        return;
    change([&](QList<IgnoreRule> &rules) {
        for (IgnoreRule &r : rules) {
            if (isFor(r, key))
                r.mode = ruleMode(mode);
        }
    });
}

void IgnoredStats::remove(const QString &key)
{
    if (!m_keys.contains(key))
        return;
    change([&key](QList<IgnoreRule> &rules) {
        rules.removeIf([&key](const IgnoreRule &rule) { return isFor(rule, key); });
    });
}

void IgnoredStats::refresh()
{
    const QList<IgnoreRule> &rules = m_active->character().ignored;
    QStringList keys;
    if (m_active->exists()) {
        for (const IgnoreRule &rule : rules) {
            if (rule.preset.isEmpty())
                keys << rule.key;
        }
        keys.removeDuplicates();
        sortStats(keys);
    }
    if (keys == m_keys) {
        if (!m_keys.isEmpty())
            Q_EMIT dataChanged(index(0), index(int(m_keys.size()) - 1), {ModeRole});
    } else {
        beginResetModel();
        m_keys = keys;
        endResetModel();
    }

    m_groups.clear();
    if (m_active->exists()) {
        for (const IgnorePreset &preset : ignorePresets()) {
            const IgnoreRule *rule = groupRule(rules, preset.id);
            m_groups << IgnoredGroup{.id = preset.id,
                                     .name = preset.label,
                                     .stats = preset.keys.join(u", "),
                                     .mode = rule ? modeOf(*rule) : Counts};
        }
    }
    Q_EMIT changed();
}

void IgnoredStats::change(const std::function<void(QList<IgnoreRule> &)> &edit)
{
    if (m_active->change([&edit](Character &character) { edit(character.ignored); }))
        setError({});
    else
        setError(m_active->errorString());
}

void IgnoredStats::setError(const QString &why)
{
    if (why == m_error)
        return;
    m_error = why;
    Q_EMIT errorChanged();
}

} // namespace pou
