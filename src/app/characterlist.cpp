// SPDX-License-Identifier: MIT
#include "app/characterlist.h"

#include "app/activecharacter.h"
#include "app/settingsstore.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// as the list shows it, "Level 92 Stormweaver"
QString heading(const PobBuild &build)
{
    QString name = buildName(build);
    if (!name.isEmpty())
        name[0] = name.at(0).toUpper();
    return name;
}

} // namespace

QString buildName(const PobBuild &build)
{
    const QString what = build.ascendancy.isEmpty() ? build.className : build.ascendancy;
    if (build.level <= 0)
        return what;
    return what.isEmpty() ? u"level %1"_s.arg(build.level) : u"level %1 %2"_s.arg(build.level).arg(what);
}

CharacterList::CharacterList(CharacterStore *store, ActiveCharacter *active, SettingsStore *settings,
                             QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
    , m_active(active)
    , m_settings(settings)
    , m_current(settings->settings().character)
{
    connect(settings, &SettingsStore::changed, this, &CharacterList::followSettings);
}

void CharacterList::load()
{
    beginResetModel();
    m_store->load();
    m_ids = m_store->ids();
    m_builds.clear();
    for (const QString &id : std::as_const(m_ids)) {
        const std::optional<QString> xml = m_store->pobBuild(id);
        if (const std::optional<PobBuild> build = xml ? parsePobBuild(*xml) : std::nullopt)
            m_builds.insert(id, heading(*build));
    }
    endResetModel();
    Q_EMIT loaded();
}

int CharacterList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_ids.size());
}

QVariant CharacterList::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const QString &id = m_ids.at(index.row());
    switch (role) {
    case IdRole:
        return id;
    case Qt::DisplayRole:
    case NameRole:
        if (const Character *character = m_store->find(id))
            return character->name;
        return {};
    case BuildRole:
        return m_builds.value(id);
    }
    return {};
}

QHash<int, QByteArray> CharacterList::roleNames() const
{
    return {
        {IdRole, "characterId"},
        {NameRole, "name"},
        {BuildRole, "build"},
    };
}

void CharacterList::setCurrent(const QString &id)
{
    if (!id.isEmpty() && !m_store->find(id))
        return;
    Settings settings = m_settings->settings();
    settings.character = id;
    m_settings->set(settings);
}

QString CharacterList::add(const QString &name)
{
    Character character;
    character.name = name.trimmed();
    if (character.name.isEmpty()) {
        fail(u"Give the character a name."_s);
        return {};
    }
    const std::optional<QString> id = m_store->add(character);
    if (!id) {
        fail(u"Couldn't save %1: %2"_s.arg(character.name, m_store->errorString()));
        return {};
    }
    const QStringList ids = m_store->ids();
    const int row = int(ids.indexOf(*id));
    beginInsertRows({}, row, row);
    m_ids = ids;
    endInsertRows();
    succeed();
    setCurrent(*id);
    return *id;
}

bool CharacterList::rename(const QString &id, const QString &name)
{
    const Character *saved = m_store->find(id);
    if (!saved)
        return false;
    Character character = *saved;
    character.name = name.trimmed();
    if (character.name.isEmpty())
        return fail(u"Give the character a name."_s);
    if (character.name == saved->name) {
        succeed();
        return true;
    }
    if (!m_store->save(id, character))
        return fail(u"Couldn't save %1: %2"_s.arg(character.name, m_store->errorString()));
    changed(id);
    succeed();
    return true;
}

bool CharacterList::remove(const QString &id)
{
    const int row = int(m_ids.indexOf(id));
    if (row < 0)
        return false;
    const QString name = m_store->find(id)->name;
    if (!m_store->remove(id))
        return fail(u"Couldn't delete %1: %2"_s.arg(name, m_store->errorString()));
    beginRemoveRows({}, row, row);
    m_ids.removeAt(row);
    m_builds.remove(id);
    endRemoveRows();
    succeed();
    if (id == m_current)
        setCurrent(m_ids.value(0));
    return true;
}

std::optional<BuildImport> CharacterList::importBuild(const QString &id, const QString &xml,
                                                      const PobBuild &build)
{
    const Character *saved = m_store->find(id);
    if (!saved)
        return std::nullopt;
    BuildImport out = pou::importBuild(*saved, build);
    // the build first, as its gear is no use without it
    if (!m_store->savePobBuild(id, xml) || !m_store->save(id, out.character)) {
        fail(u"Couldn't save the build for %1: %2"_s.arg(saved->name, m_store->errorString()));
        return std::nullopt;
    }
    m_builds.insert(id, heading(build));
    changed(id);
    succeed();
    return out;
}

void CharacterList::followSettings()
{
    const QString current = m_settings->settings().character;
    if (current == m_current)
        return;
    m_current = current;
    Q_EMIT currentChanged();
}

bool CharacterList::fail(const QString &why)
{
    if (why != m_error) {
        m_error = why;
        Q_EMIT errorChanged();
    }
    return false;
}

void CharacterList::succeed()
{
    if (!m_error.isEmpty()) {
        m_error.clear();
        Q_EMIT errorChanged();
    }
}

void CharacterList::changed(const QString &id)
{
    const QStringList ids = m_store->ids();
    const int from = int(m_ids.indexOf(id));
    const int to = int(ids.indexOf(id));
    if (from != to) {
        // moving it down puts it before the row after
        beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
        m_ids = ids;
        endMoveRows();
    }
    const QModelIndex row = index(to);
    Q_EMIT dataChanged(row, row);
    if (id == m_active->id())
        m_active->reload();
}

} // namespace pou
