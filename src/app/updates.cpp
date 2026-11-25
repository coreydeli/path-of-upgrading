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
#include <utility>

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
    , m_reopenPage(settings->settings().updates.reopenPage)
    , m_reopenHidden(settings->settings().updates.reopenHidden)
    , m_updatedTo(settings->settings().updates.updatedTo)
{
    // only the first start after the update
    if (!m_reopenPage.isEmpty() || m_reopenHidden || !m_updatedTo.isEmpty()) {
        change([](UpdateSettings &updates) {
            updates.reopenPage.clear();
            updates.reopenHidden = false;
            updates.updatedTo.clear();
        });
    }
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
    connect(checker, &UpdateChecker::found, this, [this](const Release &release) {
        if (!isPending() || m_stage != UpdateStage::Idle)
            return;
        if (canTakeByItself(release))
            download(release, true);
        else
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

bool Updates::installsByItself() const
{
    return m_settings->settings().updates.installByItself;
}

void Updates::setInstallByItself(bool install)
{
    change([&](UpdateSettings &updates) { updates.installByItself = install; });
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
    switch (m_stage) {
    case UpdateStage::Downloading:
        return u"Downloading version %1."_s.arg(newVersion());
    case UpdateStage::Installing:
        return u"Installing version %1."_s.arg(newVersion());
    case UpdateStage::Ready:
        return u"Version %1 is downloaded and ready to install."_s.arg(newVersion());
    default:
        break;
    }
    if (m_checker->isChecking())
        return u"Looking for updates."_s;
    if (!m_checker->errorString().isEmpty())
        return m_checker->errorString();
    if (!m_checker->checked().isValid())
        return {};
    if (m_checker->newest())
        return u"Version %1 is out."_s.arg(m_checker->newest()->version.text());
    return u"You're on the newest version."_s;
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

bool Updates::canTakeByItself(const Release &release) const
{
    return installsByItself() && m_installer && m_downloader && assetFor(release, m_kind) &&
           checksumsOf(release);
}

void Updates::updateNow()
{
    if (m_stage == UpdateStage::Ready) {
        install();
        return;
    }
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
    download(release, false);
}

void Updates::download(const Release &release, bool automatic)
{
    // so the window doesn't offer what's already on its way
    m_putOff = release.version.text();
    m_automatic = automatic;
    m_unasked = automatic ? release.version.text() : QString();
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
        m_unasked.clear();
        Q_EMIT changed();
        return;
    }
    if (m_automatic && m_installer->asksForPassword()) {
        // the system's prompt shouldn't come up unasked, so it waits for you
        m_automatic = false;
        m_unasked.clear();
        m_putOff.clear();
        m_stage = UpdateStage::Ready;
        Q_EMIT changed();
        Q_EMIT offered();
        return;
    }
    install();
}

void Updates::install()
{
    m_automatic = false;
    // Install on a Ready update is your click, so a failure shows in the window
    if (m_stage == UpdateStage::Ready)
        m_unasked.clear();
    m_stage = UpdateStage::Installing;
    Q_EMIT changed();
    m_installer->install(m_file, [this](const QString &error) {
        if (!error.isEmpty()) {
            if (m_installer->fallsBackToCommand()) {
                // it couldn't, so you do it: the reason and the command
                m_error = error;
                m_command = manualCommand(m_kind, m_file);
                m_stage = UpdateStage::Manual;
                m_unasked.clear();
                Q_EMIT changed();
            } else {
                fail(error);
            }
            return;
        }
        finish(m_installer->after());
    });
}

void Updates::fail(const QString &why)
{
    m_error = why;
    m_automatic = false;
    const QString unasked = std::exchange(m_unasked, QString());
    m_stage = UpdateStage::Failed;
    Q_EMIT changed();
    // the window shows it when it's open, and otherwise nobody would know
    if (!unasked.isEmpty() && !m_windowShown)
        Q_EMIT automaticFailed(unasked, why);
}

void Updates::finish(PackageInstaller::After after)
{
    // the new version opens where you were, and says it's new
    change([&](UpdateSettings &updates) {
        updates.reopenPage = m_shownPage;
        updates.reopenHidden = !m_windowShown;
        updates.updatedTo = newVersion();
    });
    if (!m_finish || m_finish(after))
        return;
    change([](UpdateSettings &updates) {
        updates.reopenPage.clear();
        updates.reopenHidden = false;
        updates.updatedTo.clear();
    });
    fail(u"Version %1 is installed, but the app couldn't restart by itself. "
         u"Quit it from the tray icon, then start it again."_s.arg(newVersion()));
}

void Updates::dismiss()
{
    if (m_stage == UpdateStage::Downloading && m_downloader)
        m_downloader->cancel();
    // the installer is already at it, and can't be stopped
    if (m_stage == UpdateStage::Installing)
        return;
    m_automatic = false;
    m_unasked.clear();
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
