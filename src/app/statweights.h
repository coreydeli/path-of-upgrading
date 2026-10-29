// SPDX-License-Identifier: MIT
#pragma once

#include "core/weights.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>
#include <optional>

namespace pou {

class ActiveCharacter;

// A weight to try for a stat your gear has.
struct WeightSuggestion
{
    Q_GADGET
    QML_VALUE_TYPE(weightSuggestion)

    Q_PROPERTY(QString key MEMBER key)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(double weight MEMBER weight)
    Q_PROPERTY(QString why MEMBER why)

public:
    QString key;
    QString name; // "Armour"
    double weight = 0;
    QString why; // "On 3 of your items, 25 on average"

    friend bool operator==(const WeightSuggestion &, const WeightSuggestion &) = default;
};

// The stat weights of the character you play, as the window shows them,
// which are the rows. The item's own stats come first. Each change is saved
// straight away.
class StatWeights : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // The name of the one you play, empty while you play nobody and there
    // are no rows.
    Q_PROPERTY(QString character READ character NOTIFY changed)
    // The weights every character starts with.
    Q_PROPERTY(bool defaults READ isDefault NOTIFY changed)
    // From the gear the character wears, the stats on the most items first.
    Q_PROPERTY(QList<pou::WeightSuggestion> suggestions READ suggestions NOTIFY changed)
    // Some suggestion is for a stat with no weight yet.
    Q_PROPERTY(bool suggestsNew READ suggestsNew NOTIFY changed)
    // What the last change that can be undone did, empty when there's none.
    Q_PROPERTY(QString undoable READ undoable NOTIFY changed)
    // Why the last change couldn't be saved, empty when it could.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1, // "[Base] Armour"
        NameRole,                   // "Armour"
        NoteRole,                   // what it is, for the item's own
        WeightRole,
    };

    // It has to outlive it.
    explicit StatWeights(ActiveCharacter *active, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString character() const;
    bool isDefault() const;
    QList<WeightSuggestion> suggestions() const { return m_suggestions; }
    bool suggestsNew() const;
    QString undoable() const;

    // Only for a stat that has a weight.
    Q_INVOKABLE void setWeight(const QString &key, double weight);
    // At 1, unless it has a weight already.
    Q_INVOKABLE void add(const QString &key);
    Q_INVOKABLE void remove(const QString &key);
    // Back to the weights every character starts with.
    Q_INVOKABLE void reset();
    // The suggestions in place of all the weights.
    Q_INVOKABLE void useSuggestions();
    // Only the suggestions for stats with no weight yet.
    Q_INVOKABLE void addSuggestions();
    Q_INVOKABLE void undo();

    QString error() const { return m_error; }

Q_SIGNALS:
    void changed();
    void errorChanged();

private:
    struct Undo
    {
        QString id; // of the character
        Weights weights;
        QString what;
    };

    void refresh();
    bool change(const std::function<void(Weights &)> &edit);
    void replace(const Weights &weights, const QString &what);
    void setError(const QString &why);

    ActiveCharacter *m_active;
    QStringList m_keys;
    QList<WeightSuggestion> m_suggestions;
    std::optional<Undo> m_undo;
    QString m_error;
};

} // namespace pou
