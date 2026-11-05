// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"

#include <QAbstractListModel>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class ItemHistory;

// The items you checked lately, as the window lists them: the newest first,
// at most the last thirty.
class CheckedItems : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // How many items it lists, for the page to say.
    Q_PROPERTY(int limit READ limit CONSTANT)

public:
    enum Role {
        NameRole = Qt::UserRole + 1, // "Storm Crown", or its base type when it has no name
        BaseRole,                    // its base type, empty when that's its name
        ClassRole,                   // "Helmets"
        RarityRole,                  // "Rare"
        ItemLevelRole,               // 0 when the item doesn't say
        RequiredLevelRole,           // 0 when it doesn't need one
    };

    // It has to outlive it.
    explicit CheckedItems(ItemHistory *history, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int limit() const;

    // What the row has to show for itself, for the page to put beside the
    // list: the roles above by their names, then "corrupted", "properties"
    // (the lines of its base properties, like "Armour: 340") and "mods", each
    // a map of "text" and "tag" (see ItemLine). Empty for a row that isn't
    // there.
    Q_INVOKABLE QVariantMap details(int row) const;

    // Has the popup show the item again, judged for the character you play
    // now. Nothing for a row that isn't there.
    Q_INVOKABLE void checkAgain(int row);
    // Empties the list. The stats of the items stay for the stat picker.
    Q_INVOKABLE void forget();

Q_SIGNALS:
    // For checkAgain().
    void checkAgainAsked(const Item &item);

private:
    ItemHistory *m_history;
};

} // namespace pou
