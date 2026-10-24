// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"

#include "core/pobeval.h"
#include "core/pobverdict.h"
#include "services/logging.h"
#include "services/pobengine.h"

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
