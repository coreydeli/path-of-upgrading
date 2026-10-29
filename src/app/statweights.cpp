// SPDX-License-Identifier: MIT
#include "app/statweights.h"

#include "app/activecharacter.h"
#include "core/autoweights.h"
#include "core/numbers.h"
#include "core/statnorm.h"

#include <QCollator>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QStringList sortedKeys(const Weights &weights)
{
    QStringList keys = weights.keys();
    // stat texts are English whatever language the app is in
    const QCollator collator{QLocale(QLocale::English)};
    std::ranges::sort(keys, [&collator](const QString &a, const QString &b) {
        if (isItemProperty(a) != isItemProperty(b))
            return isItemProperty(a);
        return collator.compare(statName(a), statName(b)) < 0;
    });
    return keys;
}

QString noteFor(const QString &key, const Character &character)
{
    if (isIgnored(character.ignored, key))
        return u"You ignore it, so it doesn't count"_s;
    if (key.startsWith(u"[Weapon] "))
        return u"The weapon's own, with its mods"_s;
    if (isItemProperty(key))
        return u"The item's own, with its mods"_s;
    return {};
}

QString whyFor(const SuggestedWeight &s, const Weights &weights)
{
    QString why = s.items == 1 ? u"On one of your items, at %1"_s.arg(formatNumber(s.average))
                               : u"On %1 of your items, %2 on average"_s.arg(QString::number(s.items),
                                                                             formatNumber(s.average));
    if (const auto it = weights.constFind(s.key); it != weights.cend())
        why += u". Your weight for it is "_s + formatNumber(*it);
    return why;
}

} // namespace

StatWeights::StatWeights(ActiveCharacter *active, QObject *parent)
    : QAbstractListModel(parent)
    , m_active(active)
{
    connect(active, &ActiveCharacter::changed, this, &StatWeights::refresh);
    refresh();
}

int StatWeights::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_keys.size());
}

QVariant StatWeights::data(const QModelIndex &index, int role) const
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
        return noteFor(key, m_active->character());
    case WeightRole:
        return m_active->character().weights.value(key);
    }
    return {};
}

QHash<int, QByteArray> StatWeights::roleNames() const
{
    return {{KeyRole, "key"}, {NameRole, "name"}, {NoteRole, "note"}, {WeightRole, "weight"}};
}

QString StatWeights::character() const
{
    return m_active->character().name;
}

bool StatWeights::isDefault() const
{
    return m_active->character().weights == defaultWeights();
}

bool StatWeights::suggestsNew() const
{
    const Weights &weights = m_active->character().weights;
    return std::ranges::any_of(m_suggestions,
                               [&weights](const WeightSuggestion &s) { return !weights.contains(s.key); });
}

QString StatWeights::undoable() const
{
    return m_undo ? m_undo->what : QString();
}

void StatWeights::setWeight(const QString &key, double weight)
{
    const Weights &weights = m_active->character().weights;
    if (!std::isfinite(weight) || !weights.contains(key) || weights.value(key) == weight)
        return;
    change([&](Weights &w) { w.insert(key, weight); });
}

void StatWeights::add(const QString &key)
{
    if (key.isEmpty() || m_active->character().weights.contains(key))
        return;
    change([&key](Weights &w) { w.insert(key, 1); });
}

void StatWeights::remove(const QString &key)
{
    if (!m_active->character().weights.contains(key))
        return;
    change([&key](Weights &w) { w.remove(key); });
}

void StatWeights::reset()
{
    if (!isDefault())
        replace(defaultWeights(), u"Your weights are back to the defaults."_s);
}

void StatWeights::useSuggestions()
{
    Weights suggested;
    for (const WeightSuggestion &s : std::as_const(m_suggestions))
        suggested.insert(s.key, s.weight);
    if (!suggested.isEmpty() && suggested != m_active->character().weights)
        replace(suggested, u"Your weights are the suggested ones now."_s);
}

void StatWeights::addSuggestions()
{
    Weights weights = m_active->character().weights;
    int added = 0;
    for (const WeightSuggestion &s : std::as_const(m_suggestions)) {
        if (!weights.contains(s.key)) {
            weights.insert(s.key, s.weight);
            ++added;
        }
    }
    if (added == 1)
        replace(weights, u"Added a suggested weight to yours."_s);
    else if (added > 1)
        replace(weights, u"Added %1 suggested weights to yours."_s.arg(added));
}

void StatWeights::undo()
{
    if (!m_undo)
        return;
    const Weights before = m_undo->weights;
    change([&before](Weights &w) { w = before; });
}

void StatWeights::refresh()
{
    const Character &character = m_active->character();
    if (m_undo && (m_undo->id != m_active->id() || !m_active->exists()))
        m_undo.reset();

    const QStringList keys = m_active->exists() ? sortedKeys(character.weights) : QStringList();
    if (keys == m_keys) {
        if (!m_keys.isEmpty())
            Q_EMIT dataChanged(index(0), index(int(m_keys.size()) - 1), {NoteRole, WeightRole});
    } else {
        beginResetModel();
        m_keys = keys;
        endResetModel();
    }

    m_suggestions.clear();
    const auto ignore = [&rules = character.ignored](const QString &key) {
        return isIgnored(rules, key);
    };
    for (const SuggestedWeight &s : suggestWeights(character.gear, ignore))
        m_suggestions << WeightSuggestion{s.key, statName(s.key), s.weight, whyFor(s, character.weights)};
    Q_EMIT changed();
}

bool StatWeights::change(const std::function<void(Weights &)> &edit)
{
    // which can't be undone once the weights have changed since, unless they
    // couldn't be saved
    std::optional<Undo> undo = std::exchange(m_undo, std::nullopt);
    if (!m_active->change([&edit](Character &character) { edit(character.weights); })) {
        m_undo = std::move(undo);
        setError(m_active->errorString());
        return false;
    }
    setError({});
    return true;
}

void StatWeights::replace(const Weights &weights, const QString &what)
{
    Undo undo{.id = m_active->id(), .weights = m_active->character().weights, .what = what};
    if (!change([&weights](Weights &w) { w = weights; }))
        return;
    m_undo = std::move(undo);
    Q_EMIT changed();
}

void StatWeights::setError(const QString &why)
{
    if (why == m_error)
        return;
    m_error = why;
    Q_EMIT errorChanged();
}

} // namespace pou
