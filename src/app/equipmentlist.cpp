// SPDX-License-Identifier: MIT
#include "app/equipmentlist.h"

#include "app/gearlist.h"
#include "core/slots.h"

namespace pou {

EquipmentList::EquipmentList(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    // so lessThan() is asked, which keeps the others in the order they came in
    sort(0);
}

QHash<int, QByteArray> EquipmentList::roleNames() const
{
    QHash<int, QByteArray> names = QSortFilterProxyModel::roleNames();
    names.insert(RowRole, "cellRow");
    names.insert(ColumnRole, "cellColumn");
    return names;
}

QVariant EquipmentList::data(const QModelIndex &index, int role) const
{
    if (role != RowRole && role != ColumnRole)
        return QSortFilterProxyModel::data(index, role);
    const auto cell = equipmentCell(QSortFilterProxyModel::data(index, GearList::SlotRole).toString());
    if (!cell)
        return -1;
    return role == RowRole ? cell->row : cell->column;
}

void EquipmentList::setExtras(bool extras)
{
    if (extras == m_extras)
        return;
    m_extras = extras;
    invalidate();
    Q_EMIT extrasChanged();
}

void EquipmentList::setOnly(const QString &slot)
{
    if (slot == m_only)
        return;
    m_only = slot;
    invalidate();
    Q_EMIT onlyChanged();
}

bool EquipmentList::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    if (!sourceModel())
        return false;
    const QModelIndex index = sourceModel()->index(row, 0, parent);
    if (!m_only.isEmpty())
        return sourceModel()->data(index, GearList::SlotRole).toString() == m_only;
    return sourceModel()->data(index, GearList::ExtraRole).toBool() == m_extras;
}

bool EquipmentList::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto a = equipmentCell(sourceModel()->data(left, GearList::SlotRole).toString());
    const auto b = equipmentCell(sourceModel()->data(right, GearList::SlotRole).toString());
    if (!a || !b)
        return false;
    return a->row != b->row ? a->row < b->row : a->column < b->column;
}

} // namespace pou
