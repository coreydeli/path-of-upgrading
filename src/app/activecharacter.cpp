// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"

#include "core/pobeval.h"
#include "core/pobverdict.h"
#include "core/wearing.h"
#include "services/logging.h"
#include "services/pobengine.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

ActiveCharacter::ActiveCharacter(CharacterStore *store, PobEngine *pob, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_pob(pob)
{
    connect(m_pob, &PobEngine::buildLoaded, this, &ActiveCharacter::loaded);
}

void ActiveCharacter::setId(const QString &id)
{
    m_id = id;
    reload();
}

void ActiveCharacter::reload()
{
    const Character *character = m_store->find(m_id);
    m_character = character ? *character : Character();
    const std::optional<QString> xml = character ? m_store->pobBuild(m_id) : std::nullopt;
    m_build = xml ? parsePobBuild(*xml) : std::nullopt;
    if (m_build)
        m_pob->setBuild(*xml, pobWearing(m_character.gear, m_character.fromPob, m_build->swapInUse));
    else
        m_pob->setBuild({});
    Q_EMIT changed();
}

bool ActiveCharacter::wear(const QString &slot, const Item &item)
{
    const Character *saved = m_store->find(m_id);
    if (!saved)
        return fail(u"There's no character to wear it. Add one first."_s);
    const std::optional<Character> worn = pou::wear(*saved, slot, item);
    if (!worn)
        return fail(u"%1 can't go in %2."_s.arg(item.name, slot));
    return save(*worn);
}

bool ActiveCharacter::takeOff(const QString &slot)
{
    const Character *saved = m_store->find(m_id);
    if (!saved || !saved->gear.contains(slot))
        return true;
    return save(pou::takeOff(*saved, slot));
}

bool ActiveCharacter::change(const std::function<void(Character &)> &edit)
{
    const Character *saved = m_store->find(m_id);
    if (!saved)
        return fail(u"There's no character to change. Add one first."_s);
    Character character = *saved;
    edit(character);
    return save(character);
}

bool ActiveCharacter::save(const Character &character)
{
    if (!m_store->save(m_id, character))
        return fail(u"Couldn't save %1: %2"_s.arg(character.name, m_store->errorString()));
    m_error.clear();
    // which has Path of Building wear it too
    reload();
    return true;
}

bool ActiveCharacter::fail(const QString &why)
{
    m_error = why;
    return false;
}

void ActiveCharacter::loaded(const QJsonObject &summary)
{
    const bool chaosInoculation = hasChaosInoculation(readPobStats(summary.value(u"stats").toObject()));
    const Character *saved = m_store->find(m_id);
    if (!saved || chaosInoculation == saved->chaosInoculation)
        return;
    Character character = *saved;
    character.chaosInoculation = chaosInoculation;
    // and when it can't be, it's found out again the next time
    if (!m_store->save(m_id, character))
        qCWarning(lcPob, "Couldn't save what Path of Building found out about the build: %s",
                  qUtf8Printable(m_store->errorString()));
    m_character.chaosInoculation = chaosInoculation;
    Q_EMIT changed();
}

} // namespace pou
