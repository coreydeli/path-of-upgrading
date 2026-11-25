// SPDX-License-Identifier: MIT
#pragma once

#include "core/poeapi.h"

#include <QAbstractListModel>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class CharacterList;
class PoeAccount;
class PobEngine;
class PoeClient;
struct AccountCharacter;

// The characters on the linked Path of Exile account, as the Characters page
// lists them to pick one to import. Every league is shown, as the website
// gives them, and a deleted or expired character isn't.
class AccountCharacters : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    Q_PROPERTY(bool loading READ isLoading NOTIFY changed)
    // Why the list isn't there, or empty. When the website says to wait, how
    // long.
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // Whether the last ask got an answer, even an empty list.
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY changed)
    // While one is being fetched to import.
    Q_PROPERTY(bool importing READ isImporting NOTIFY changed)
    // What the last import did, as a sentence, or why it didn't.
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    enum Role {
        NameRole = Qt::UserRole + 1, // "Stormcaller"
        SummaryRole,                 // "Level 92 Stormweaver, Standard"
        LevelRole,
        ClassRole,
        LeagueRole,
        CurrentRole,  // the one last played
        ImportedRole, // it's one of yours already, so it updates
    };

    // They have to outlive it.
    AccountCharacters(PoeClient *client, PoeAccount *account, CharacterList *characters, PobEngine *pob,
                      QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Asks the website for the list. Does nothing while it's asking, or when
    // there's no link.
    Q_INVOKABLE void refresh();

    bool isLoading() const { return m_loading; }
    QString error() const { return m_error; }
    // Fetches the character in that row and puts what it wears on one of
    // yours, new or the one linked to it already. One at a time, and only
    // when asked.
    Q_INVOKABLE void importCharacter(int row);

    bool isImporting() const { return m_importing; }
    QString message() const { return m_message; }
    bool isLoaded() const { return m_loaded; }
    // Nothing for a row that isn't there.
    AccountCharacter at(int row) const { return m_list.value(row); }

Q_SIGNALS:
    void changed();
    // The id of the character it went to.
    void imported(const QString &id);

private:
    void clear();
    void finishImport(const AccountCharacter &picked, const QJsonObject &character);
    void finishWith(const QString &message);

    PoeClient *m_client;
    PoeAccount *m_account;
    CharacterList *m_characters;
    PobEngine *m_pob;
    QList<AccountCharacter> m_list;
    bool m_loading = false;
    bool m_loaded = false;
    bool m_importing = false;
    QString m_message;
    QString m_error;
};

} // namespace pou
