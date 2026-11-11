// SPDX-License-Identifier: MIT
#include "app/updates.h"

#include "app/settingsstore.h"
#include "core/updates.h"
#include "platform/opener.h"
#include "services/updatechecker.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

Updates::Updates(UpdateChecker *checker, SettingsStore *settings, Opener *opener, QObject *parent)
    : QObject(parent)
    , m_checker(checker)
    , m_settings(settings)
    , m_opener(opener)
{
    connect(checker, &UpdateChecker::changed, this, &Updates::changed);
    connect(settings, &SettingsStore::changed, this, &Updates::changed);
    connect(checker, &UpdateChecker::found, this, [this] {
        if (isPending())
            Q_EMIT offered();
    });
}

void Updates::change(const std::function<void(UpdateSettings &)> &edit)
{
    Settings settings = m_settings->settings();
    edit(settings.updates);
    // which does nothing when it's the same
    m_settings->set(settings);
}

bool Updates::checksOnLaunch() const
{
    return m_settings->settings().updates.checkOnLaunch;
}

void Updates::setCheckOnLaunch(bool check)
{
    change([&](UpdateSettings &updates) { updates.checkOnLaunch = check; });
}

bool Updates::includesBetas() const
{
    return m_settings->settings().updates.includeBetas;
}

void Updates::setIncludeBetas(bool include)
{
    change([&](UpdateSettings &updates) { updates.includeBetas = include; });
}

QString Updates::version() const
{
    return m_checker->have().text();
}

bool Updates::isChecking() const
{
    return m_checker->isChecking();
}

QString Updates::status() const
{
    if (m_checker->isChecking())
        return u"Looking for updates."_s;
    if (!m_checker->errorString().isEmpty())
        return m_checker->errorString();
    if (!m_checker->checked().isValid())
        return {};
    if (m_checker->newest())
        return u"Version %1 is out."_s.arg(m_checker->newest()->version.text());
    return u"You have the newest version."_s;
}

bool Updates::isPending() const
{
    return m_checker->isOffered() && m_putOff != m_checker->newest()->version.text();
}

QString Updates::newVersion() const
{
    return m_checker->newest() ? m_checker->newest()->version.text() : QString();
}

QString Updates::notes() const
{
    return m_checker->newest() ? plainNotes(m_checker->newest()->notes) : QString();
}

void Updates::checkNow()
{
    m_putOff.clear();
    // you asked, so a version you skipped comes up again
    if (!m_settings->settings().updates.skipped.isEmpty())
        change([](UpdateSettings &updates) { updates.skipped.clear(); });
    m_checker->check();
    Q_EMIT changed();
}

void Updates::updateNow()
{
    if (!m_checker->newest())
        return;
    m_putOff = m_checker->newest()->version.text();
    if (m_opener)
        m_opener->open(m_checker->newest()->page);
    Q_EMIT changed();
}

void Updates::later()
{
    if (!m_checker->newest())
        return;
    m_putOff = m_checker->newest()->version.text();
    Q_EMIT changed();
}

void Updates::skip()
{
    if (!m_checker->newest())
        return;
    const QString skipped = m_checker->newest()->version.text();
    change([&](UpdateSettings &updates) { updates.skipped = skipped; });
}

} // namespace pou
