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
    };

    // Says how it went: empty when it worked, or why it didn't.
    using Done = std::function<void(const QString &error)>;

    virtual ~PackageInstaller() = default;

    virtual After after() const = 0;
    // Whether the system asks for your password, so the app waits for you to
    // say go rather than start it unasked.
    virtual bool asksForPassword() const { return false; }
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

// The AppImage an update replaced, kept next to the new one as "<name>.old"
// until the new one has run for a bit. A start that never gets that far leaves
// a marker, and the next start puts the old one back.
class OldAppImage
{
public:
    // The path of the AppImage that is running.
    explicit OldAppImage(const QString &appImage);

    // The old file's name, and the marker's.
    QString oldPath() const;
    QString markerPath() const;
    // Puts the old one back if the last start of the new one didn't get to
    // confirm itself. True when it did, and the caller starts the program again.
    bool restoreIfUnconfirmed();
    // This start has begun, which stays on record until it confirms. Does
    // nothing when there's no old file or the marker can't be written.
    void begin();
    // The new one works: the old file and the marker go. Only the start that
    // began does this, as the old version quits to restart and mustn't take
    // the file it just kept.
    void confirm();

private:
    QString m_appImage;
    bool m_began = false;
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

// Installs an Arch package with pacman, through pkexec, which asks for your
// password in the desktop's own prompt. Without pkexec, or when the prompt is
// turned away or pacman fails, there's the command to run by hand.
class PacmanInstaller : public PackageInstaller
{
public:
    // Runs a program and says whether it started and how it ended.
    using Finished = std::function<void(bool started, int exitCode)>;
    using Run = std::function<void(const QString &program, const QStringList &arguments, const Finished &)>;

    // Starts the process with QProcess, unless it's given another way.
    explicit PacmanInstaller(const Run &run = {});

    After after() const override { return After::Restart; }
    bool asksForPassword() const override { return true; }
    bool fallsBackToCommand() const override { return true; }
    void install(const QString &file, const Done &done) override;

private:
    Run m_run;
};

// The one for how the program is installed, or none when it can't do it by
// itself.
std::unique_ptr<PackageInstaller> createPackageInstaller(InstallKind kind, const QString &appImagePath);

} // namespace pou
