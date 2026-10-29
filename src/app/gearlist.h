// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <optional>

namespace pou {

class ActiveCharacter;
class SettingsStore;

// What the character you play wears, as the window lists it: each of the
// main slots, worn or not, then the second weapon set, flasks, charms and
// jewels it has on.
class GearList : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // The name of the one you play, empty while you play nobody and the list
    // is empty.
    Q_PROPERTY(QString character READ character NOTIFY changed)
    // It has some on besides the main slots.
    Q_PROPERTY(bool hasExtras READ hasExtras NOTIFY changed)
    // The gear score over the main slots, "56", and its grade, "B". Empty
    // when none of them has a gear score.
    Q_PROPERTY(QString overall READ overall NOTIFY changed)
    Q_PROPERTY(QString overallGrade READ overallGrade NOTIFY changed)
    // The three main slots with the lowest gear scores, "Gloves 31, Boots 40,
    // Belt 44".
    Q_PROPERTY(QString weakest READ weakest NOTIFY changed)
    // Why the last change couldn't be made, empty when it could.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Role {
        SlotRole = Qt::UserRole + 1, // "Ring 1"
        NameRole,                    // empty when there's nothing on
        BaseRole,                    // its base type, empty when that's its name
        FromPobRole,                 // it came with the build
        ScoreRole,                   // its gear score, "78", empty when it has none
        GradeRole,                   // "A"
        ExtraRole,                   // not one of the main slots
    };

    // They have to outlive it.
    GearList(ActiveCharacter *active, SettingsStore *settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString character() const;
    bool hasExtras() const;
    QString overall() const { return m_overall; }
    QString overallGrade() const { return m_overallGrade; }
    QString weakest() const { return m_weakest; }

    Q_INVOKABLE bool takeOff(const QString &slot);

    QString error() const { return m_error; }

Q_SIGNALS:
    void changed();
    void errorChanged();

private:
    struct Row
    {
        QString slot;
        std::optional<int> score;
        QString grade;
    };

    void refresh();
    void setError(const QString &why);

    ActiveCharacter *m_active;
    SettingsStore *m_settings;
    QList<Row> m_rows;
    PobPriorities m_priorities; // what the scores were worked out with
    QString m_overall;
    QString m_overallGrade;
    QString m_weakest;
    QString m_error;
};

} // namespace pou
