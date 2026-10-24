// SPDX-License-Identifier: MIT
#include "app/poboptions.h"

#include "app/settingsstore.h"
#include "services/pobengine.h"

#include <algorithm>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {

PobOptions::PobOptions(SettingsStore *settings, PobEngine *pob, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_pob(pob)
{
    connect(settings, &SettingsStore::changed, this, &PobOptions::changed);
    connect(pob, &PobEngine::installChanged, this, &PobOptions::changed);
    connect(pob, &PobEngine::stateChanged, this, &PobOptions::changed);
}

const PobSettings &PobOptions::pob() const
{
    return m_settings->settings().pob;
}

template<typename Edit>
void PobOptions::change(Edit edit)
{
    Settings settings = m_settings->settings();
    edit(settings.pob);
    // which does nothing when it's the same
    m_settings->set(settings);
}

void PobOptions::setEnabled(bool enabled)
{
    change([&](PobSettings &pob) { pob.enabled = enabled; });
}

void PobOptions::setLocation(const QString &location)
{
    change([&](PobSettings &pob) { pob.location = location.trimmed(); });
}

void PobOptions::setLocationUrl(const QUrl &folder)
{
    if (folder.isLocalFile())
        setLocation(folder.toLocalFile());
}

void PobOptions::setDamageWeight(double weight)
{
    if (std::isfinite(weight))
        change([&](PobSettings &pob) { pob.damageWeight = std::clamp(weight, 0.0, 1.0); });
}

void PobOptions::setResistsFirst(bool first)
{
    change([&](PobSettings &pob) { pob.priorities.resistsFirst = first; });
}

void PobOptions::setChaos(bool chaos)
{
    change([&](PobSettings &pob) { pob.priorities.chaos = chaos; });
}

void PobOptions::setKeepDamage(bool keep)
{
    change([&](PobSettings &pob) { pob.priorities.keepDamage = keep; });
}

void PobOptions::setSpirit(bool spirit)
{
    change([&](PobSettings &pob) { pob.priorities.spirit = spirit; });
}

void PobOptions::setAttributes(bool attributes)
{
    change([&](PobSettings &pob) { pob.priorities.attributes = attributes; });
}

QString PobOptions::found() const
{
    const std::optional<PobInstall> &install = m_pob->install();
    if (!install)
        return {};
    if (install->flatpak)
        return u"Using the Path of Building Flatpak."_s;
    return u"Using Path of Building in %1."_s.arg(install->folder);
}

QString PobOptions::problem() const
{
    const PobEngine::State state = m_pob->state();
    if (state == PobEngine::State::Unavailable || state == PobEngine::State::Failed)
        return m_pob->errorString();
    return {};
}

} // namespace pou
