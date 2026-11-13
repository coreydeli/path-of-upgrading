// SPDX-License-Identifier: MIT
#include "app/poboptions.h"

#include "app/settingsstore.h"
#include "platform/opener.h"
#include "services/pobengine.h"
#include "services/pobfinder.h"

#include <algorithm>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {

PobOptions::PobOptions(SettingsStore *settings, PobEngine *pob, const PobPlaces &places, Opener *opener,
                       QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_pob(pob)
    , m_places(places)
    , m_opener(opener)
{
    connect(settings, &SettingsStore::changed, this, &PobOptions::follow);
    connect(pob, &PobEngine::installChanged, this, &PobOptions::follow);
    connect(pob, &PobEngine::stateChanged, this, &PobOptions::follow);
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

void PobOptions::setKeepDefence(bool keep)
{
    change([&](PobSettings &pob) { pob.priorities.keepDefence = keep; });
}

void PobOptions::setSpirit(bool spirit)
{
    change([&](PobSettings &pob) { pob.priorities.spirit = spirit; });
}

void PobOptions::setAttributes(bool attributes)
{
    change([&](PobSettings &pob) { pob.priorities.attributes = attributes; });
}

const PobGuide &PobOptions::guide() const
{
    if (!m_guide) {
        const PobEngine::State state = m_pob->state();
        const QString runProblem = state == PobEngine::State::Failed ? m_pob->errorString() : QString();
        const std::optional<PobInstall> &running = m_pob->install();
        m_guide = guideToPob(m_places, pob().location, running, runProblem,
                             surveyPob(m_places, pob().location, running));
    }
    return *m_guide;
}

void PobOptions::follow()
{
    m_guide.reset();
    Q_EMIT changed();
}

QString PobOptions::installCommand() const
{
    return pobInstallCommand();
}

void PobOptions::openDiscover()
{
    if (m_opener)
        m_opener->open(QUrl(pobDiscoverUrl()));
}

void PobOptions::openFlathub()
{
    if (m_opener)
        m_opener->open(QUrl(pobFlathubUrl()));
}

void PobOptions::openInstaller()
{
    if (m_opener)
        m_opener->open(QUrl(pobInstallerUrl()));
}

void PobOptions::copyInstallCommand()
{
    if (m_opener)
        m_opener->copy(pobInstallCommand());
}

void PobOptions::useUsualPlaces()
{
    setLocation({});
}

} // namespace pou
