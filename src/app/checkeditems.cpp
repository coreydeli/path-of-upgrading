// SPDX-License-Identifier: MIT
#include "app/checkeditems.h"

#include "app/itemhistory.h"

using namespace Qt::Literals::StringLiterals;

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

int CheckedItems::limit() const
{
    return int(ItemHistory::recentLimit);
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

QVariantMap CheckedItems::details(int row) const
{
    if (row < 0 || row >= rowCount())
        return {};
    const Item &item = m_history->recent().at(row);
    const QModelIndex at = index(row);
    QVariantList properties;
    QVariantList mods;
    for (const ItemLine &line : item.lines) {
        const bool property = line.tag == u"base" || line.tag == u"info";
        (property ? properties : mods) << QVariantMap{{u"text"_s, line.text}, {u"tag"_s, line.tag}};
    }
    return {
        {u"name"_s, data(at, NameRole)},
        {u"base"_s, data(at, BaseRole)},
        {u"itemClass"_s, data(at, ClassRole)},
        {u"rarity"_s, data(at, RarityRole)},
        {u"itemLevel"_s, data(at, ItemLevelRole)},
        {u"requiredLevel"_s, data(at, RequiredLevelRole)},
        {u"corrupted"_s, item.corrupted},
        {u"properties"_s, properties},
        {u"mods"_s, mods},
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
