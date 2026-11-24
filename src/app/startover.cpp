// SPDX-License-Identifier: MIT
#include "app/startover.h"

#include "app/tradesession.h"
#include "platform/autostart.h"
#include "platform/globalshortcuts.h"
#include "platform/secretstore.h"
#include "services/ownpaths.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

StartOver::StartOver(bool offered, const Places &places, Autostart *autostart, SecretStore *secrets,
                     GlobalShortcuts *shortcuts, QObject *parent)
    : QObject(parent)
    , m_offered(offered)
    , m_places(places)
    , m_autostart(autostart)
    , m_secrets(secrets)
    , m_shortcuts(shortcuts)
    , m_secretNames{{.name = TradeSession::secretName(),
                     .problem = u"Couldn't remove your session ID from the keyring: %1"_s}}
{}

void StartOver::addSecret(const QString &name, const QString &problem)
{
    m_secretNames << Secret{.name = name, .problem = problem};
}

void StartOver::run()
{
    if (!m_offered || m_busy)
        return;
    m_busy = true;
    m_problems.clear();
    Q_EMIT changed();

    m_shortcuts->forgetAll();
    if (!m_autostart->setEnabled(false))
        m_problems << m_autostart->errorString();
    // the files go once it answers, so nothing is saved over them in between
    removeSecret(0);
}

void StartOver::removeSecret(qsizetype index)
{
    if (index >= m_secretNames.size()) {
        removeFiles();
        return;
    }
    m_secrets->remove(m_secretNames.at(index).name, this, [this, index](const SecretStore::Result &result) {
        if (result.status == SecretStore::Status::Failed)
            m_problems << m_secretNames.at(index).problem.arg(result.error);
        removeSecret(index + 1);
    });
}

void StartOver::removeFiles()
{
    m_problems << removeOwnPaths({{.what = u"your settings"_s, .path = m_places.settings},
                                  {.what = u"your characters and items"_s, .path = m_places.data},
                                  {.what = u"the saved site data and downloads"_s,
                                   .path = ownFolderOf(m_places.cache, m_places.marker)}},
                                 m_places.marker);
    m_busy = false;
    if (m_problems.isEmpty() && (!m_restart || !m_restart()))
        m_problems << u"Everything is gone, but the app couldn't start again by itself. "
                      "Quit it from the tray icon, then start it again."_s;
    Q_EMIT changed();
}

} // namespace pou
