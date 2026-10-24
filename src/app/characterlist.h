// SPDX-License-Identifier: MIT
#pragma once

#include "core/buildimport.h"
#include "core/characterstore.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>
#include <optional>

namespace pou {

class ActiveCharacter;
class SettingsStore;

// What the build is, as in a sentence: "level 92 Stormweaver". Empty when it
// doesn't say.
QString buildName(const PobBuild &build);

// Your characters as the window lists them, by name, and the one you play.
// They're changed through here, so the list keeps up.
class CharacterList : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // The id of the one you play, empty when there's none.
    Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY currentChanged)
    // The files that couldn't be read, with why, see CharacterStore::problems().
    Q_PROPERTY(QStringList problems READ problems NOTIFY loaded)
    // Why the last change couldn't be made, empty when it could.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1, // "characterId", as QML has its own id
        NameRole,
        BuildRole, // "Level 92 Stormweaver", empty without a build
    };

    // They have to outlive it. It's empty until it's loaded.
    CharacterList(CharacterStore *store, ActiveCharacter *active, SettingsStore *settings,
                  QObject *parent = nullptr);

    // Reads them from the folder again.
    void load();
    QStringList problems() const { return m_store->problems(); }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString current() const { return m_current; }
    void setCurrent(const QString &id);

    // A new character wearing nothing, which you then play. Its id, or empty
    // when it couldn't be saved.
    Q_INVOKABLE QString add(const QString &name);
    Q_INVOKABLE bool rename(const QString &id, const QString &name);
    // Along with its build. When it's the one you play, you play the first of
    // the others.
    Q_INVOKABLE bool remove(const QString &id);

    // Saves the build for the character, with its gear on, see importBuild().
    // Empty when it couldn't be saved.
    std::optional<BuildImport> importBuild(const QString &id, const QString &xml, const PobBuild &build);

    QString error() const { return m_error; }

Q_SIGNALS:
    void currentChanged();
    void errorChanged();
    void loaded();

private:
    void followSettings();
    bool fail(const QString &why);
    void succeed();
    // after it's been changed in the store, which can move it in the list
    void changed(const QString &id);

    CharacterStore *m_store;
    ActiveCharacter *m_active;
    SettingsStore *m_settings;
    QStringList m_ids;
    QHash<QString, QString> m_builds; // what each id's build is, see BuildRole
    QString m_current;
    QString m_error;
};

} // namespace pou
