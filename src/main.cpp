// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "app/commandline.h"
#include "app/desktop.h"
#include "core/pobinstall.h"
#include "platform/gameinput.h"
#include "platform/globalshortcuts.h"
#include "platform/notifier.h"
#include "platform/secretstore.h"
#include "platform/tray.h"

#include <KDBusService>
#include <KLocalizedQmlContext>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QWindow>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;

namespace {

bool styleInstalled(const QString &uri)
{
    const QString relative = QString(uri).replace(u'.', u'/');
    QStringList dirs =
        qEnvironmentVariable("QML_IMPORT_PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    dirs << QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    for (const QString &dir : std::as_const(dirs)) {
        if (QFileInfo::exists(dir + u'/' + relative + u"/qmldir"))
            return true;
    }
    return false;
}

// Follow the Plasma look when qqc2-desktop-style is around, otherwise use
// Fusion, which at least picks up the platform palette.
void pickControlsStyle()
{
    if (!QQuickStyle::name().isEmpty())
        return;
    const QString desktop = u"org.kde.desktop"_s;
    QQuickStyle::setStyle(styleInstalled(desktop) ? desktop : u"Fusion"_s);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"path-of-upgrading"_s);
    QApplication::setApplicationDisplayName(u"Path of Upgrading"_s);
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(APP_ID));
    // with the application name, this makes the bus name io.github.coreydeli.path-of-upgrading
    QApplication::setOrganizationDomain(u"coreydeli.github.io"_s);
    // closing the window leaves it in the tray
    QApplication::setQuitOnLastWindowClosed(false);

    // --help and --version print and exit here
    pou::CommandLine commandLine;
    commandLine.process(app.arguments());

    // Started a second time, this hands over to the first and exits, before
    // anything below is set up.
    KDBusService service(KDBusService::Unique);

    pickControlsStyle();

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    // which goes before the engine its popup is from
    pou::App root(&engine, pou::createGameInput(), pou::createGlobalShortcuts(), pou::createSecretStore(),
                  std::make_unique<QNetworkAccessManager>(), pou::pobPlaces(), pou::App::defaultPaths());
    const std::unique_ptr<pou::Tray> tray = pou::createTray();
    const std::unique_ptr<pou::Notifier> notifier = pou::createNotifier();
    pou::Desktop desktop(tray.get(), notifier.get(), root.overlayOptions(), &root.hotkeys());
    QObject::connect(&service, &KDBusService::activateRequested, &desktop, &pou::Desktop::startedAgain);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.setInitialProperties(
        {{u"app"_s, QVariant::fromValue(&root)}, {u"startHidden"_s, commandLine.background()}});
    engine.loadFromModule("PathOfUpgrading", "Main");
    desktop.setWindow(qobject_cast<QWindow *>(engine.rootObjects().value(0)));
    desktop.reportProblems();

    return app.exec();
}
