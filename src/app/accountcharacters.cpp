// SPDX-License-Identifier: MIT
#include "app/accountcharacters.h"

#include "app/poeaccount.h"
#include "services/poeclient.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

AccountCharacters::AccountCharacters(PoeClient *client, PoeAccount *account, QObject *parent)
    : QAbstractListModel(parent)
    , m_client(client)
    , m_account(account)
{
    // another account's characters aren't for this one
    connect(account, &PoeAccount::linkedChanged, this, [this] {
        if (!m_account->isLinked())
            clear();
    });
}

int AccountCharacters::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_list.size());
}

QVariant AccountCharacters::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const AccountCharacter &c = m_list.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return c.name;
    case SummaryRole: {
        QString text = u"Level %1 %2"_s.arg(c.level).arg(c.className);
        if (!c.league.isEmpty())
            text += u", "_s + c.league;
        return text;
    }
    case LevelRole:
        return c.level;
    case ClassRole:
        return c.className;
    case LeagueRole:
        return c.league;
    case CurrentRole:
        return c.current;
    }
    return {};
}

QHash<int, QByteArray> AccountCharacters::roleNames() const
{
    return {
        {NameRole, "name"},       {SummaryRole, "summary"}, {LevelRole, "level"},
        {ClassRole, "className"}, {LeagueRole, "league"},   {CurrentRole, "current"},
    };
}

void AccountCharacters::clear()
{
    beginResetModel();
    m_list.clear();
    endResetModel();
    m_loaded = false;
    m_loading = false;
    m_error.clear();
    Q_EMIT changed();
}

void AccountCharacters::refresh()
{
    if (m_loading || !m_account->isLinked())
        return;
    m_loading = true;
    m_error.clear();
    Q_EMIT changed();
    m_client->characters(this, [this](const PoeResponse &response) {
        m_loading = false;
        if (!m_account->isLinked()) {
            // unlinked while it was out
            clear();
            return;
        }
        if (!response.ok) {
            m_error = response.failure.text;
            Q_EMIT changed();
            return;
        }
        beginResetModel();
        m_list = readCharacterList(response.body);
        endResetModel();
        m_loaded = true;
        Q_EMIT changed();
    });
}

} // namespace pou
