// SPDX-License-Identifier: MIT
#include "app/onboarding.h"

#include "app/characterlist.h"
#include "app/overlayoptions.h"
#include "app/settingsstore.h"
#include "core/firstrun.h"

#include <QKeySequence>

namespace pou {

Onboarding::Onboarding(SettingsStore *settings, CharacterList *characters, OverlayOptions *options,
                       QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_characters(characters)
    , m_options(options)
{
    connect(options, &OverlayOptions::changed, this, &Onboarding::hotkeyChanged);
}

void Onboarding::decide()
{
    setPending(setupWanted(m_settings->settings(), m_characters->rowCount() > 0));
}

void Onboarding::ask()
{
    setPending(true);
}

void Onboarding::finish()
{
    setPending(false);
    Settings settings = m_settings->settings();
    settings.setupDone = true;
    // which does nothing when it's saved already
    m_settings->set(settings);
}

QString Onboarding::hotkey() const
{
    return m_options->checkKey().toString(QKeySequence::NativeText);
}

void Onboarding::setPending(bool pending)
{
    if (pending == m_pending)
        return;
    m_pending = pending;
    Q_EMIT pendingChanged();
}

} // namespace pou
