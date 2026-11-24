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
    // the old one stays until the new one has run for a while
    const OldAppImage old(m_target);
    QFile::remove(old.oldPath());
    QFile::remove(old.markerPath());
    std::error_code error;
    std::filesystem::rename(m_target.toStdString(), old.oldPath().toStdString(), error);
    if (error) {
        QFile::remove(staged);
        done(u"Couldn't keep the old %1: %2"_s.arg(target.fileName(),
                                                   QString::fromStdString(error.message())));
        return;
    }
    // replaces it in one step, so there's never half an AppImage
    std::filesystem::rename(staged.toStdString(), m_target.toStdString(), error);
    if (error) {
        // back as it was
        std::error_code ignored;
        std::filesystem::rename(old.oldPath().toStdString(), m_target.toStdString(), ignored);
        QFile::remove(staged);
        done(u"Couldn't replace %1: %2"_s.arg(target.fileName(), QString::fromStdString(error.message())));
        return;
    }
    done({});
}

OldAppImage::OldAppImage(const QString &appImage)
    : m_appImage(appImage)
{}

QString OldAppImage::oldPath() const
{
    return m_appImage + u".old"_s;
}

QString OldAppImage::markerPath() const
{
    return m_appImage + u".starting"_s;
}

bool OldAppImage::restoreIfUnconfirmed()
{
    if (m_appImage.isEmpty() || !QFile::exists(oldPath()) || !QFile::exists(markerPath()))
        return false;
    QFile::remove(markerPath());
    std::error_code error;
    // over the new one, which is the file that's running, and stays so until it quits
    std::filesystem::rename(oldPath().toStdString(), m_appImage.toStdString(), error);
    return !error;
}

void OldAppImage::begin()
{
    if (m_appImage.isEmpty() || !QFile::exists(oldPath()))
        return;
    QFile marker(markerPath());
    if (!marker.open(QIODevice::WriteOnly))
        return;
    marker.close();
    m_began = true;
}

void OldAppImage::confirm()
{
    if (!m_began)
        return;
    m_began = false;
    QFile::remove(markerPath());
    QFile::remove(oldPath());
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

PacmanInstaller::PacmanInstaller(const Run &run)
    : m_run(run)
{
    if (!m_run) {
        m_run = [](const QString &program, const QStringList &arguments, const Finished &finished) {
            auto *process = new QProcess;
            QObject::connect(process, &QProcess::errorOccurred, process, [process, finished](auto error) {
                if (error != QProcess::FailedToStart)
                    return;
                finished(false, -1);
                process->deleteLater();
            });
            QObject::connect(process, &QProcess::finished, process, [process, finished](int exitCode) {
                finished(true, exitCode);
                process->deleteLater();
            });
            process->start(program, arguments);
        };
    }
}

void PacmanInstaller::install(const QString &file, const Done &done)
{
    m_run(u"pkexec"_s, {u"pacman"_s, u"-U"_s, u"--noconfirm"_s, file}, [done](bool started, int exitCode) {
        if (!started)
            done(u"pkexec isn't installed, so the app can't ask for your password."_s);
        // pkexec says 126 when the prompt was dismissed and 127 when it was refused
        else if (exitCode == 126 || exitCode == 127)
            done(u"The password prompt was turned away."_s);
        else if (exitCode != 0)
            done(u"pacman couldn't install it (it stopped with code %1)."_s.arg(exitCode));
        else
            done({});
    });
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
    // PackageKit's alpm backend isn't known to take a local package file, so
    // Arch goes through pkexec and pacman
    case InstallKind::Arch:
        return std::make_unique<PacmanInstaller>();
#endif
    default:
        return nullptr;
    }
}

} // namespace pou
