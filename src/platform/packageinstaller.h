// SPDX-License-Identifier: MIT
#pragma once

#include "core/updates.h"

#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

namespace pou {

// Puts a downloaded and checked update in place of the program. Only the kinds
// of install that can do it without a terminal have one.
class PackageInstaller
{
public:
    // What the program does once it's in place.
    enum class After {
        Restart, // the new program starts as this one goes
        Quit,    // the installer starts the new program
        Offer,   // the new one is in place, and the app asks you to restart
    };

    // Says how it went: empty when it worked, or why it didn't.
    using Done = std::function<void(const QString &error)>;

    virtual ~PackageInstaller() = default;

    virtual After after() const = 0;
    // Whether a failure shows the command to run by hand, in place of the error.
    virtual bool fallsBackToCommand() const { return false; }
    // The file is one the downloader has checked.
    virtual void install(const QString &file, const Done &done) = 0;
};

// Replaces the AppImage you started with the new one, in the same folder.
class AppImageInstaller : public PackageInstaller
{
public:
    // The path of the AppImage that is running.
    explicit AppImageInstaller(const QString &target);

    After after() const override { return After::Restart; }
    void install(const QString &file, const Done &done) override;

private:
    QString m_target;
};

// Runs the new setup program without asking anything, which installs it for
// you, closes the old one's files and starts the new program.
class SetupInstaller : public PackageInstaller
{
public:
    using Launch = std::function<bool(const QString &program, const QStringList &arguments)>;

    // Starts the setup with QProcess, unless it's given another way.
    explicit SetupInstaller(const Launch &launch = {});

    After after() const override { return After::Quit; }
    void install(const QString &file, const Done &done) override;

private:
    Launch m_launch;
};

// The one for how the program is installed, or none when it can't do it by
// itself.
std::unique_ptr<PackageInstaller> createPackageInstaller(InstallKind kind, const QString &appImagePath);

} // namespace pou
