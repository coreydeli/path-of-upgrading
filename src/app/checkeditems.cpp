// SPDX-License-Identifier: MIT
#include "app/checkeditems.h"

#include "app/itemhistory.h"

namespace pou {

CheckedItems::CheckedItems(ItemHistory *history, QObject *parent)
    : QAbstractListModel(parent)
    , m_history(history)
{
    // it's a short list, and an item checked again moves up it
    connect(history, &ItemHistory::changed, this, [this] {
        beginResetModel();
        endResetModel();
    });
}

int CheckedItems::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_history->recent().size());
}

QVariant CheckedItems::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Item &item = m_history->recent().at(index.row());
    const QString &name = item.name.isEmpty() ? item.baseType : item.name;
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return name;
    case BaseRole:
        return item.baseType != name ? item.baseType : QString();
    case ClassRole:
        return item.itemClass;
    case RarityRole:
        return item.rarity;
    case ItemLevelRole:
        return item.itemLevel;
    case RequiredLevelRole:
        return item.requiredLevel;
    }
    return {};
}

QHash<int, QByteArray> CheckedItems::roleNames() const
{
    return {
        {NameRole, "name"},     {BaseRole, "base"},           {ClassRole, "itemClass"},
        {RarityRole, "rarity"}, {ItemLevelRole, "itemLevel"}, {RequiredLevelRole, "requiredLevel"},
    };
}

void CheckedItems::checkAgain(int row)
{
    if (row < 0 || row >= rowCount())
        return;
    // a copy, as checking it moves it in the history
    const Item item = m_history->recent().at(row);
    Q_EMIT checkAgainAsked(item);
}

void CheckedItems::forget()
{
    m_history->clearRecent();
}

} // namespace pou
