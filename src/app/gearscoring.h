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

// How the gear score rates the gear of the character you play, as the window
// shows it: by which build profile, and how much each of its stats counts,
// which are the rows. Each change is saved straight away.
class GearScoring : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // The name of the one you play, empty while you play nobody and there
    // are no rows.
    Q_PROPERTY(QString character READ character NOTIFY changed)
    // The build profiles' names, then none, for your stat weights alone.
    Q_PROPERTY(QStringList profiles READ profiles CONSTANT)
    // Choosing one yourself, even the one it has, stops importing a build from
    // changing it.
    Q_PROPERTY(int profile READ profile WRITE setProfile NOTIFY changed)
    // What the profile is for.
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    // How much a stat can count, from not at all to three times as much.
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
    explicit GearScoring(ActiveCharacter *active, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString character() const;
    QStringList profiles() const;
    int profile() const;
    void setProfile(int profile);
    QString summary() const;
    QStringList levels() const;
    bool isTweaked() const;

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
    QString m_profile; // the one the rows are for
    QList<TweakableStat> m_stats;
    QString m_error;
};

} // namespace pou
