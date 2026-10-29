// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "app/commandline.h"
#include "core/pobinstall.h"
#include "platform/gameinput.h"
#include "platform/globalshortcuts.h"
#include "platform/secretstore.h"

#include <KLocalizedQmlContext>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>

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

    // --help and --version print and exit here
    pou::CommandLine commandLine;
    commandLine.process(app.arguments());

    pickControlsStyle();

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    // which goes before the engine its popup is from
    pou::App root(&engine, pou::createGameInput(), pou::createGlobalShortcuts(), pou::createSecretStore(),
                  std::make_unique<QNetworkAccessManager>(), pou::pobPlaces(), pou::App::defaultPaths());
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.setInitialProperties({{u"app"_s, QVariant::fromValue(&root)}});
    engine.loadFromModule("PathOfUpgrading", "Main");

    return app.exec();
}
