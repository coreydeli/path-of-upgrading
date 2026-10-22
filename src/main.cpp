// SPDX-License-Identifier: MIT

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

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

    pickControlsStyle();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("PathOfUpgrading", "Main");

    return app.exec();
}
