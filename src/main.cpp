// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "app/commandline.h"
#include "app/desktop.h"
#include "app/fonts.h"
#include "app/ownstyle.h"
#include "platform/gameinput.h"
#include "platform/gamewatcher.h"
#include "platform/globalshortcuts.h"
#include "platform/notifier.h"
#include "platform/opener.h"
#include "platform/packageinstaller.h"
#include "platform/restart.h"
#include "platform/secretstore.h"
#include "platform/singleinstance.h"
#include "platform/tray.h"
#include "services/pobfinder.h"

#include <KLocalizedQmlContext>
#include <QApplication>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QWindow>

#ifdef Q_OS_WIN
#include <KIconTheme>
#include <QIcon>
#endif

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Windows has no icon themes, so Breeze comes with the app
    KIconTheme::initTheme();
#endif
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"path-of-upgrading"_s);
    QApplication::setApplicationDisplayName(u"Path of Upgrading"_s);
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(APP_ID));
    // with the application name, this makes the bus name io.github.coreydeli.path-of-upgrading
    QApplication::setOrganizationDomain(u"coreydeli.github.io"_s);
    // closing the window leaves it in the tray
    QApplication::setQuitOnLastWindowClosed(false);
    pou::addBundledFonts();
#ifdef Q_OS_WIN
    // which the desktop file gives it elsewhere
    QApplication::setWindowIcon(QIcon(u":/icons/" APP_ID ".svg"_s));
#endif

    // --help and --version print and exit here
    pou::CommandLine commandLine;
    commandLine.process(app.arguments());

    // Started a second time, this hands over to the first and exits, before
    // anything below is set up.
    const std::unique_ptr<pou::SingleInstance> instance = pou::runOnce();

    pou::useOwnStyle();

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    // made after the engine so it goes first, as its popup and results windows come from the engine
    pou::App root(&engine, pou::createGameInput(), pou::createGlobalShortcuts(), pou::createSecretStore(),
                  pou::createOpener(), std::make_unique<QNetworkAccessManager>(), pou::pobPlaces(),
                  pou::App::defaultPaths(), pou::createGameWatcher());
    const std::unique_ptr<pou::Tray> tray = pou::createTray();
    const std::unique_ptr<pou::Notifier> notifier = pou::createNotifier(tray.get());
    pou::Desktop desktop(tray.get(), notifier.get(), root.overlayOptions(), &root.hotkeys());
    QObject::connect(instance.get(), &pou::SingleInstance::startedAgain, &desktop,
                     &pou::Desktop::startedAgain);
    QObject::connect(&desktop, &pou::Desktop::setupAsked, root.onboarding(), &pou::Onboarding::ask);
    if (commandLine.setup())
        root.onboarding()->ask();
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.setInitialProperties(
        {{u"app"_s, QVariant::fromValue(&root)}, {u"startHidden"_s, commandLine.background()}});
    engine.loadFromModule("PathOfUpgrading", "Main");
    QObject::connect(root.updates(), &pou::Updates::offered, &desktop,
                     [&] { desktop.tellAboutUpdate(root.updates()->newVersion()); });
    desktop.setWindow(qobject_cast<QWindow *>(engine.rootObjects().value(0)));
    desktop.reportProblems();
    root.setUpdateInstaller(pou::createPackageInstaller(pou::installedKind(qEnvironmentVariable("APPIMAGE")),
                                                        qEnvironmentVariable("APPIMAGE")),
                            [](pou::PackageInstaller::After after) {
                                // and it stays, to say so
                                if (after == pou::PackageInstaller::After::Restart &&
                                    !pou::startAgainAfterExit())
                                    return false;
                                QCoreApplication::quit();
                                return true;
                            });
    root.updateChecker().start();

    const int exitCode = app.exec();
    // The main window reads from the app, so it goes before the app does.
    qDeleteAll(engine.rootObjects());
    return exitCode;
}
