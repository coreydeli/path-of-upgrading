// SPDX-License-Identifier: MIT
#include "app/gearlist.h"

#include "app/activecharacter.h"
#include "app/settingsstore.h"
#include "core/character.h"
#include "core/evaluation.h"
#include "core/slots.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {

GearList::GearList(ActiveCharacter *active, SettingsStore *settings, QObject *parent)
    : QAbstractListModel(parent)
    , m_active(active)
    , m_settings(settings)
    , m_priorities(settings->settings().pob.priorities)
{
    connect(active, &ActiveCharacter::changed, this, &GearList::refresh);
    // which rate resistances higher when they come first
    connect(settings, &SettingsStore::changed, this, [this] {
        if (m_settings->settings().pob.priorities != m_priorities)
            refresh();
    });
    refresh();
}

int GearList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant GearList::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Row &row = m_rows.at(index.row());
    const Character &character = m_active->character();
    const auto item = character.gear.constFind(row.slot);
    const bool worn = item != character.gear.cend();
    switch (role) {
    case Qt::DisplayRole:
    case SlotRole:
        return row.slot;
    case NameRole:
        return worn ? item->name : QString();
    case BaseRole:
        return worn && item->baseType != item->name ? item->baseType : QString();
    case FromPobRole:
        return character.fromPob.contains(row.slot);
    case ScoreRole:
        return row.score ? QString::number(*row.score) : QString();
    case GradeRole:
        return row.grade;
    case ExtraRole:
        return !mainSlots().contains(row.slot);
    }
    return {};
}

QHash<int, QByteArray> GearList::roleNames() const
{
    return {
        {SlotRole, "slot"},   {NameRole, "name"},   {BaseRole, "base"},   {FromPobRole, "fromPob"},
        {ScoreRole, "score"}, {GradeRole, "grade"}, {ExtraRole, "extra"},
    };
}

QString GearList::character() const
{
    return m_active->character().name;
}

bool GearList::hasExtras() const
{
    return m_rows.size() > mainSlots().size();
}

bool GearList::takeOff(const QString &slot)
{
    if (!m_active->takeOff(slot)) {
        setError(m_active->errorString());
        return false;
    }
    setError({});
    return true;
}

void GearList::refresh()
{
    m_priorities = m_settings->settings().pob.priorities;
    const Character &character = m_active->character();

    beginResetModel();
    m_rows.clear();
    if (m_active->exists()) {
        for (const QString &slot : allSlots()) {
            const auto item = character.gear.constFind(slot);
            if (item == character.gear.cend()) {
                if (mainSlots().contains(slot))
                    m_rows << Row{.slot = slot};
                continue;
            }
            Row row{.slot = slot};
            const std::optional<GearScore> score =
                gearScore(*item, gearScoreOptions(character, slot, m_priorities));
            if (score && score->isRated()) {
                row.score = score->score;
                row.grade = score->grade;
            }
            m_rows << row;
        }
    }

    // the scores worked out above
    const GearScoreFn slotScore = [this](const Item &, const QString &slot) -> std::optional<double> {
        for (const Row &row : std::as_const(m_rows)) {
            if (row.slot == slot)
                return row.score;
        }
        return std::nullopt;
    };
    const std::optional<double> overall = overallGearScore(character.gear, slotScore);
    m_overall = overall ? QString::number(*overall) : QString();
    m_overallGrade = overall ? gearGrade(int(*overall)) : QString();

    QList<const Row *> rated;
    for (const Row &row : std::as_const(m_rows)) {
        if (row.score && mainSlots().contains(row.slot))
            rated << &row;
    }
    std::stable_sort(rated.begin(), rated.end(),
                     [](const Row *a, const Row *b) { return *a->score < *b->score; });
    QStringList weakest;
    for (const Row *row : rated.first(std::min<qsizetype>(3, rated.size())))
        weakest << row->slot + u' ' + QString::number(*row->score);
    m_weakest = weakest.join(u", "_s);
    endResetModel();
    Q_EMIT changed();
}

void GearList::setError(const QString &why)
{
    if (why == m_error)
        return;
    m_error = why;
    Q_EMIT errorChanged();
}

} // namespace pou
