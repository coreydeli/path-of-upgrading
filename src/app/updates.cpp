// SPDX-License-Identifier: MIT
#include "app/updates.h"

#include "app/settingsstore.h"
#include "core/updates.h"
#include "platform/opener.h"
#include "services/updatechecker.h"
#include "services/updatedownloader.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QUrl>

using namespace Qt::Literals::StringLiterals;

namespace pou {

Updates::Updates(UpdateChecker *checker, SettingsStore *settings, Opener *opener,
                 UpdateDownloader *downloader, QObject *parent)
    : QObject(parent)
    , m_checker(checker)
    , m_settings(settings)
    , m_opener(opener)
    , m_downloader(downloader)
    , m_kind(installedKind(qEnvironmentVariable("APPIMAGE")))
{
    if (downloader) {
        connect(downloader, &UpdateDownloader::progress, this, [this](qint64 received, qint64 total) {
            m_progress = total > 0 ? double(received) / double(total) : -1;
            Q_EMIT changed();
        });
        connect(downloader, &UpdateDownloader::done, this, &Updates::downloaded);
        connect(downloader, &UpdateDownloader::failed, this, &Updates::fail);
    }
    connect(checker, &UpdateChecker::changed, this, &Updates::changed);
    connect(settings, &SettingsStore::changed, this, &Updates::changed);
    connect(checker, &UpdateChecker::found, this, [this] {
        if (isPending())
            Q_EMIT offered();
    });
}

Updates::~Updates() = default;

void Updates::setInstallKind(InstallKind kind)
{
    m_kind = kind;
}

void Updates::setInstaller(std::unique_ptr<PackageInstaller> installer, const Finish &finish)
{
    m_installer = std::move(installer);
    m_finish = finish;
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
    if (!m_checker->newest() || m_stage == UpdateStage::Downloading || m_stage == UpdateStage::Installing)
        return;
    const Release release = *m_checker->newest();
    m_putOff = release.version.text();
    if (!m_downloader || !assetFor(release, m_kind) || !checksumsOf(release)) {
        // nothing it can fetch and check, so GitHub has it
        if (m_opener)
            m_opener->open(release.page);
        Q_EMIT changed();
        return;
    }
    m_error.clear();
    m_command.clear();
    m_progress = -1;
    m_stage = UpdateStage::Downloading;
    m_downloader->start(release, m_kind);
    Q_EMIT changed();
}

void Updates::downloaded(const QString &file)
{
    m_file = file;
    if (!m_installer) {
        // PackageKit or a terminal does these, which the app doesn't ask for
        m_command = manualCommand(m_kind, file);
        m_stage = UpdateStage::Manual;
        Q_EMIT changed();
        return;
    }
    m_stage = UpdateStage::Installing;
    Q_EMIT changed();
    m_installer->install(file, [this](const QString &error) {
        if (!error.isEmpty()) {
            if (m_installer->fallsBackToCommand()) {
                // it couldn't, so you do it: the reason and the command
                m_error = error;
                m_command = manualCommand(m_kind, m_file);
                m_stage = UpdateStage::Manual;
                Q_EMIT changed();
            } else {
                fail(error);
            }
            return;
        }
        if (m_installer->after() == PackageInstaller::After::Offer) {
            m_stage = UpdateStage::Installed;
            Q_EMIT changed();
        } else if (m_finish) {
            m_finish(m_installer->after());
        }
    });
}

void Updates::fail(const QString &why)
{
    m_error = why;
    m_stage = UpdateStage::Failed;
    Q_EMIT changed();
}

void Updates::restart()
{
    if (m_stage == UpdateStage::Installed && m_finish)
        m_finish(PackageInstaller::After::Restart);
}

void Updates::dismiss()
{
    if (m_stage == UpdateStage::Downloading && m_downloader)
        m_downloader->cancel();
    // the installer is already at it, and can't be stopped
    if (m_stage == UpdateStage::Installing)
        return;
    m_stage = UpdateStage::Idle;
    m_error.clear();
    m_command.clear();
    Q_EMIT changed();
}

void Updates::copyCommand()
{
    if (m_opener && !m_command.isEmpty())
        m_opener->copy(m_command);
}

void Updates::openFolder()
{
    if (m_opener && !m_file.isEmpty())
        m_opener->open(QUrl::fromLocalFile(QFileInfo(m_file).absolutePath()));
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
