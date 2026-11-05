// SPDX-License-Identifier: MIT
#pragma once

#include <QSortFilterProxyModel>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

// The gear list as the equipment screen draws it: either the main slots, in
// the order they're read across the screen, or the others worn (the second
// weapon set, flasks, charms and jewels), or the one slot that's picked.
// Give it the gear list as its sourceModel. The main slots also say which row
// and column of the screen they go in.
class EquipmentList : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    // The others rather than the main slots.
    Q_PROPERTY(bool extras READ extras WRITE setExtras NOTIFY extrasChanged)
    // Just that slot, whichever kind it is, and no rows when it isn't in the
    // list. Empty leaves the choice to extras.
    Q_PROPERTY(QString only READ only WRITE setOnly NOTIFY onlyChanged)

public:
    enum Role {
        RowRole = Qt::UserRole + 100, // on the screen, -1 for the others
        ColumnRole,
    };

    explicit EquipmentList(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role) const override;

    bool extras() const { return m_extras; }
    void setExtras(bool extras);
    QString only() const { return m_only; }
    void setOnly(const QString &slot);

Q_SIGNALS:
    void extrasChanged();
    void onlyChanged();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    bool m_extras = false;
    QString m_only;
};

} // namespace pou
