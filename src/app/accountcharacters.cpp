// SPDX-License-Identifier: MIT
#include "app/accountcharacters.h"

#include "app/characterlist.h"
#include "app/poeaccount.h"
#include "core/accountitems.h"
#include "services/poeclient.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

namespace {

QString itemCount(int count)
{
    return count == 1 ? u"1 item"_s : u"%1 items"_s.arg(count);
}

} // namespace

AccountCharacters::AccountCharacters(PoeClient *client, PoeAccount *account, CharacterList *characters,
                                     QObject *parent)
    : QAbstractListModel(parent)
    , m_client(client)
    , m_account(account)
    , m_characters(characters)
{
    // another account's characters aren't for this one
    connect(account, &PoeAccount::linkedChanged, this, [this] {
        if (!m_account->isLinked())
            clear();
    });
    // which of them are yours changes with your characters
    const auto refreshImported = [this] {
        if (!m_list.isEmpty())
            Q_EMIT dataChanged(index(0), index(int(m_list.size()) - 1), {ImportedRole});
    };
    connect(characters, &CharacterList::countChanged, this, refreshImported);
    connect(characters, &CharacterList::loaded, this, refreshImported);
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
    case ImportedRole:
        return !m_characters->linkedTo(c.id).isEmpty();
    }
    return {};
}

QHash<int, QByteArray> AccountCharacters::roleNames() const
{
    return {
        {NameRole, "name"},         {SummaryRole, "summary"}, {LevelRole, "level"},
        {ClassRole, "className"},   {LeagueRole, "league"},   {CurrentRole, "current"},
        {ImportedRole, "imported"},
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
    m_message.clear();
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

void AccountCharacters::importCharacter(int row)
{
    if (m_importing || !m_account->isLinked() || row < 0 || row >= m_list.size())
        return;
    const AccountCharacter picked = m_list.at(row);
    m_importing = true;
    m_message.clear();
    Q_EMIT changed();
    m_client->character(picked.name, this, [this, picked](const PoeResponse &response) {
        m_importing = false;
        if (!m_account->isLinked()) {
            clear();
            return;
        }
        if (!response.ok) {
            m_message = response.failure.text;
            Q_EMIT changed();
            return;
        }
        const std::optional<QJsonObject> character = readCharacter(response.body);
        if (!character) {
            m_message = u"%1 isn't on your account any more."_s.arg(picked.name);
            Q_EMIT changed();
            return;
        }
        const Equipped gear = equipmentOf(*character);
        const AccountLink link{
            .realm = u"poe2"_s, .id = picked.id.isEmpty() ? picked.name : picked.id, .name = picked.name};
        const QString id = m_characters->importAccountGear(link, gear);
        if (id.isEmpty()) {
            m_message = m_characters->error();
        } else {
            m_message = u"%1 now wears %2 from the game."_s.arg(picked.name, itemCount(int(gear.size())));
            Q_EMIT imported(id);
        }
        Q_EMIT changed();
    });
}

} // namespace pou
