// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"
#include "core/updates.h"
#include "platform/packageinstaller.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>
#include <memory>

namespace pou {

class Opener;
class SettingsStore;
class UpdateChecker;
class UpdateDownloader;

// Where taking an update has got to.
namespace UpdateStage {
Q_NAMESPACE
QML_ELEMENT

enum Stage {
    Idle,
    Downloading,
    Installing,
    Manual,    // downloaded, and you run the command
    Installed, // a package is in, and the app asks to restart
    Failed,
};
Q_ENUM_NS(Stage)

} // namespace UpdateStage

// What the window says about updates: whether to look for them, whether betas
// count, the result of the last look in words, and the newer release when
// there is one, with the choices of what to do about it. The settings are
// saved as soon as they change.
class Updates : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    Q_PROPERTY(bool checkOnLaunch READ checksOnLaunch WRITE setCheckOnLaunch NOTIFY changed)
    Q_PROPERTY(bool includeBetas READ includesBetas WRITE setIncludeBetas NOTIFY changed)
    // "0.7.0" or "0.7.0-beta.4", the one you have
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool checking READ isChecking NOTIFY changed)
    // What the last look found, or why it didn't, in words. Empty before the
    // first look.
    Q_PROPERTY(QString status READ status NOTIFY changed)
    // Whether there's a release to offer, which you haven't skipped or put off
    Q_PROPERTY(bool pending READ isPending NOTIFY changed)
    Q_PROPERTY(QString newVersion READ newVersion NOTIFY changed)
    // What's new in it, as plain text
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(pou::UpdateStage::Stage stage READ stage NOTIFY changed)
    // How much of the download has come, 0 to 1, or -1 when the size isn't known
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    // Why the download or the install failed, or why the command is shown
    Q_PROPERTY(QString installError READ installError NOTIFY changed)
    // For the kinds of install that need a terminal: the command to run
    Q_PROPERTY(QString command READ command NOTIFY changed)

public:
    // They have to outlive it. The opener may be null, and then it opens nothing.
    Updates(UpdateChecker *checker, SettingsStore *settings, Opener *opener,
            UpdateDownloader *downloader = nullptr, QObject *parent = nullptr);
    ~Updates() override;

    // How the program was installed, which decides what Update Now does.
    // The app's own is found from the build and the environment.
    void setInstallKind(InstallKind kind);
    // What puts a downloaded update in place, and what happens next: the
    // program restarts or quits, as the installer says. Without one, the
    // kinds that need a terminal show the command.
    using Finish = std::function<void(PackageInstaller::After)>;
    void setInstaller(std::unique_ptr<PackageInstaller> installer, const Finish &finish);

    bool checksOnLaunch() const;
    void setCheckOnLaunch(bool check);
    bool includesBetas() const;
    void setIncludeBetas(bool include);
    QString version() const;
    bool isChecking() const;
    QString status() const;
    bool isPending() const;
    QString newVersion() const;
    QString notes() const;
    UpdateStage::Stage stage() const { return m_stage; }
    double progress() const { return m_progress; }
    QString installError() const { return m_error; }
    QString command() const { return m_command; }

    // Looks now, and offers the release again if it was skipped or put off.
    Q_INVOKABLE void checkNow();
    // Takes the newer release. It downloads the file for how the program was
    // installed and checks it, then installs it, or shows the command for it.
    // With no file for that, it opens the release's page on GitHub.
    Q_INVOKABLE void updateNow();
    // After a package is installed: starts the new version.
    Q_INVOKABLE void restart();
    // Stops a download, or closes what it said.
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void copyCommand();
    // Shows the downloaded file in the file manager.
    Q_INVOKABLE void openFolder();
    // Leaves it for the next time the program starts.
    Q_INVOKABLE void later();
    // Doesn't offer this version again. A newer one is offered.
    Q_INVOKABLE void skip();

Q_SIGNALS:
    void changed();
    // A look found a release to offer, which the window shows or, with the
    // window hidden, the desktop tells you about.
    void offered();

private:
    void change(const std::function<void(UpdateSettings &)> &edit);
    void downloaded(const QString &file);
    void fail(const QString &why);

    UpdateChecker *m_checker;
    SettingsStore *m_settings;
    Opener *m_opener;
    QString m_putOff; // the version you said Later to
    UpdateDownloader *m_downloader;
    InstallKind m_kind;
    std::unique_ptr<PackageInstaller> m_installer;
    Finish m_finish;
    UpdateStage::Stage m_stage = UpdateStage::Idle;
    double m_progress = -1;
    QString m_error;
    QString m_command;
    QString m_file;
};

} // namespace pou
