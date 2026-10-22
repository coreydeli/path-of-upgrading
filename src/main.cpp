// SPDX-License-Identifier: MIT

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

namespace {

bool styleInstalled(const QString &uri)
{
    const QString relative = QString(uri).replace(QLatin1Char('.'), QLatin1Char('/'));
    QStringList dirs = qEnvironmentVariable("QML_IMPORT_PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    dirs << QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    for (const QString &dir : std::as_const(dirs)) {
        if (QFileInfo::exists(dir + QLatin1Char('/') + relative + QLatin1String("/qmldir")))
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
    const QString desktop = QStringLiteral("org.kde.desktop");
    QQuickStyle::setStyle(styleInstalled(desktop) ? desktop : QStringLiteral("Fusion"));
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("path-of-upgrading"));
    QApplication::setApplicationDisplayName(QStringLiteral("Path of Upgrading"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(APP_ID));

    pickControlsStyle();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("PathOfUpgrading", "Main");

    return app.exec();
}
