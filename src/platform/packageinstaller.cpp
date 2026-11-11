// SPDX-License-Identifier: MIT
#include "platform/packageinstaller.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <filesystem>

#ifndef Q_OS_WIN
#include "platform/packagekitinstaller.h"
#endif

using namespace Qt::Literals::StringLiterals;

namespace pou {

AppImageInstaller::AppImageInstaller(const QString &target)
    : m_target(target)
{}

void AppImageInstaller::install(const QString &file, const Done &done)
{
    const QFileInfo target(m_target);
    if (m_target.isEmpty() || !target.isFile()) {
        done(u"Can't find the AppImage that is running."_s);
        return;
    }
    // next to it, so the last step is a rename inside one folder
    const QString staged = m_target + u".new"_s;
    QFile::remove(staged);
    if (!QFile::copy(file, staged)) {
        done(u"Couldn't copy the update next to %1. Is the folder writable?"_s.arg(target.fileName()));
        return;
    }
    QFile::setPermissions(staged, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                      QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                      QFileDevice::ReadOther | QFileDevice::ExeOther);
    // replaces it in one step, so there's never half an AppImage
    std::error_code error;
    std::filesystem::rename(staged.toStdString(), m_target.toStdString(), error);
    if (error) {
        QFile::remove(staged);
        done(u"Couldn't replace %1: %2"_s.arg(target.fileName(), QString::fromStdString(error.message())));
        return;
    }
    done({});
}

SetupInstaller::SetupInstaller(const Launch &launch)
    : m_launch(launch)
{
    if (!m_launch) {
        m_launch = [](const QString &program, const QStringList &arguments) {
            return QProcess::startDetached(program, arguments);
        };
    }
}

void SetupInstaller::install(const QString &file, const Done &done)
{
    // /S asks nothing, and /START has the setup start the program when it's done
    if (!m_launch(file, {u"/S"_s, u"/START"_s})) {
        done(u"Couldn't start the setup program."_s);
        return;
    }
    done({});
}

std::unique_ptr<PackageInstaller> createPackageInstaller(InstallKind kind, const QString &appImagePath)
{
    switch (kind) {
    case InstallKind::AppImage:
        return std::make_unique<AppImageInstaller>(appImagePath);
    case InstallKind::Windows:
        return std::make_unique<SetupInstaller>();
#ifndef Q_OS_WIN
    case InstallKind::Debian:
    case InstallKind::Fedora:
        return std::make_unique<PackageKitInstaller>();
#endif
    // Arch has no installer: PackageKit's alpm backend isn't known to take a
    // local package file, so it stays on pacman's command
    default:
        return nullptr;
    }
}

} // namespace pou
