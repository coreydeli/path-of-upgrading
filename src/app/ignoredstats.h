// SPDX-License-Identifier: MIT
#pragma once

#include "core/ignore.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>

namespace pou {

class ActiveCharacter;

// Some stats to ignore all at once.
struct IgnoredGroup
{
    Q_GADGET
    QML_VALUE_TYPE(ignoredGroup)

    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString stats MEMBER stats)
    Q_PROPERTY(int mode MEMBER mode)

public:
    QString id;
    QString name;  // "Elemental resistances"
    QString stats; // "#% to Fire Resistance, #% to Cold Resistance"
    int mode = 0;  // an IgnoredStats::Mode

    friend bool operator==(const IgnoredGroup &, const IgnoredGroup &) = default;
};

// The stats the character you play ignores, as the window shows them: some
// groups of stats, and the stats ignored one at a time, which are the rows.
// Each change is saved straight away.
class IgnoredStats : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // The name of the one you play, empty while you play nobody and there
    // are no rows.
    Q_PROPERTY(QString character READ character NOTIFY changed)
    // What each mode is called, in order.
    Q_PROPERTY(QStringList modes READ modes CONSTANT)
    Q_PROPERTY(QList<pou::IgnoredGroup> groups READ groups NOTIFY changed)
    // Every stat it ignores, in a group or by itself.
    Q_PROPERTY(QStringList keys READ keys NOTIFY changed)
    // Why the last change couldn't be saved, empty when it could.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Mode {
        Counts,
        Ignored,  // left out of scores, the popup and every search
        Excluded, // and trade searches skip listings that have it
    };
    Q_ENUM(Mode)

    enum Role {
        KeyRole = Qt::UserRole + 1, // "[Base] Armour"
        NameRole,                   // "Armour"
        NoteRole,                   // what it is, for the item's own
        ModeRole,                   // Ignored or Excluded
    };

    // It has to outlive it.
    explicit IgnoredStats(ActiveCharacter *active, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString character() const;
    QStringList modes() const;
    QList<IgnoredGroup> groups() const { return m_groups; }
    QStringList keys() const;

    Q_INVOKABLE void setGroupMode(const QString &id, int mode);
    // Ignored, unless it's ignored by itself already.
    Q_INVOKABLE void add(const QString &key);
    // Only Ignored or Excluded, and only for a stat ignored by itself.
    Q_INVOKABLE void setMode(const QString &key, int mode);
    Q_INVOKABLE void remove(const QString &key);

    QString error() const { return m_error; }

Q_SIGNALS:
    void changed();
    void errorChanged();

private:
    void refresh();
    void change(const std::function<void(QList<IgnoreRule> &)> &edit);
    void setError(const QString &why);

    ActiveCharacter *m_active;
    QStringList m_keys; // of the rows
    QList<IgnoredGroup> m_groups;
    QString m_error;
};

} // namespace pou
