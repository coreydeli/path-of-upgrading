// SPDX-License-Identifier: MIT
#pragma once

#include "core/buildprofiles.h"
#include "core/characterstore.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>

namespace pou {

class ActiveCharacter;
class BuildWeigher;

// How the gear score rates the gear of the character you play, as the window
// shows it: what it comes from, and how much each of its stats counts, which
// are the rows. Each change is saved straight away.
class GearScoring : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // The name of the one you play, empty while you play nobody and there
    // are no rows.
    Q_PROPERTY(QString character READ character NOTIFY changed)
    // Where the gear score comes from, in words: weighed by Path of Building,
    // being weighed, or a general start until it has.
    Q_PROPERTY(QString state READ state NOTIFY changed)
    // More about it: what the profile it starts from is for.
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    // When Path of Building weighed the build, as a date for the user's
    // locale, empty when it hasn't.
    Q_PROPERTY(QString weighedOn READ weighedOn NOTIFY changed)
    // Path of Building is weighing the build now.
    Q_PROPERTY(bool weighing READ isWeighing NOTIFY changed)
    // It can be asked to weigh again.
    Q_PROPERTY(bool canWeigh READ canWeigh NOTIFY changed)
    // Items are judged by your stat weights alone, with no gear score.
    Q_PROPERTY(bool weightsOnly READ weightsOnly WRITE setWeightsOnly NOTIFY changed)
    // How much a stat can count, from not at all to three times as much,
    // each with its factor: "Normal (1x)".
    Q_PROPERTY(QStringList levels READ levels CONSTANT)
    // Some stat counts for more or less than the profile has it.
    Q_PROPERTY(bool tweaked READ isTweaked NOTIFY changed)
    // Why the last change couldn't be saved, empty when it could.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Role {
        LabelRole = Qt::UserRole + 1, // "Life"
        WhereRole,                    // "Counts on Boots"
        LevelRole,                    // one of levels()
    };

    // It has to outlive it.
    GearScoring(ActiveCharacter *active, BuildWeigher *weigher, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString character() const;
    QString state() const;
    QString summary() const;
    QString weighedOn() const;
    bool isWeighing() const;
    bool canWeigh() const;
    bool weightsOnly() const;
    void setWeightsOnly(bool on);
    QStringList levels() const;
    bool isTweaked() const;

    // Has Path of Building weigh the build again, even if it has already.
    Q_INVOKABLE void weighAgain();
    Q_INVOKABLE void setLevel(const QString &label, int level);
    // Every stat back to how the profile has it.
    Q_INVOKABLE void reset();

    QString error() const { return m_error; }

Q_SIGNALS:
    void changed();
    void errorChanged();

private:
    void refresh();
    void change(const std::function<void(Character &)> &edit);
    double untweaked(const QString &label) const;
    int levelOf(const QString &label) const;
    void setError(const QString &why);

    ActiveCharacter *m_active;
    BuildWeigher *m_weigher;
    QList<TweakableStat> m_stats;
    QString m_error;
};

} // namespace pou
