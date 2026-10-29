// SPDX-License-Identifier: MIT
#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class ItemHistory;
class TradeDataStore;

// The stats to pick from, to weight or to ignore: the item's own, every mod
// the trade site knows, and those on the items you've checked. The item's own
// come first, then the ones on the most of your items. The rows are those
// with every word you search for.
class StatPicker : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // The words a stat's name has to have, in any case.
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    // The stats picked already, which can't be again.
    Q_PROPERTY(QStringList taken READ taken WRITE setTaken NOTIFY takenChanged)
    // Why the trade site's stats were missing when it last gathered them,
    // empty when they weren't.
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1, // "[Base] Armour"
        NameRole,                   // "Armour"
        NoteRole,                   // "Explicit, Rune. On 3 of the items you've checked"
        TakenRole,
    };

    // Both have to outlive it.
    StatPicker(TradeDataStore *data, ItemHistory *history, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const { return m_query; }
    void setQuery(const QString &query);
    QStringList taken() const { return m_taken; }
    void setTaken(const QStringList &keys);
    QString message() const { return m_message; }

    // Whether it may ask the trade site for its stats, which is only while
    // there are trade searches. Otherwise it has the ones kept from before.
    void setSiteAllowed(bool allowed);

    // Gathers the stats again, as when the picker opens. The trade site's
    // come later when they have to be asked for.
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void queryChanged();
    void takenChanged();
    void messageChanged();

private:
    struct Stat
    {
        QString key;
        QString name;
        QString note;
        bool property = false; // one of the item's own
        int seen = 0;          // on how many of the items you've checked
    };

    void rebuild();
    void filter();
    QString missingStats() const;
    void updateMessage();

    TradeDataStore *m_data;
    ItemHistory *m_history;
    bool m_siteAllowed = false;
    bool m_waiting = false; // for the trade site's stats
    QString m_query;
    QStringList m_words; // of the query
    QStringList m_taken;
    QList<Stat> m_stats;
    QList<qsizetype> m_rows; // the stats shown
    QString m_message;
};

} // namespace pou
