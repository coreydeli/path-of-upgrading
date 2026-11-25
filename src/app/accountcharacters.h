// SPDX-License-Identifier: MIT
#pragma once

#include "core/poeapi.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class PoeAccount;
class PoeClient;

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

public:
    enum Role {
        NameRole = Qt::UserRole + 1, // "Stormcaller"
        SummaryRole,                 // "Level 92 Stormweaver, Standard"
        LevelRole,
        ClassRole,
        LeagueRole,
        CurrentRole, // the one last played
    };

    // They have to outlive it.
    AccountCharacters(PoeClient *client, PoeAccount *account, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Asks the website for the list. Does nothing while it's asking, or when
    // there's no link.
    Q_INVOKABLE void refresh();

    bool isLoading() const { return m_loading; }
    QString error() const { return m_error; }
    bool isLoaded() const { return m_loaded; }
    // Nothing for a row that isn't there.
    AccountCharacter at(int row) const { return m_list.value(row); }

Q_SIGNALS:
    void changed();

private:
    void clear();

    PoeClient *m_client;
    PoeAccount *m_account;
    QList<AccountCharacter> m_list;
    bool m_loading = false;
    bool m_loaded = false;
    QString m_error;
};

} // namespace pou
