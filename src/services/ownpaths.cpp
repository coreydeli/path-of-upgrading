// SPDX-License-Identifier: MIT
#include "services/ownpaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

bool isSafe(const QString &path)
{
    if (path.trimmed().isEmpty())
        return false;
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    return clean != QDir::rootPath() && clean != QDir::cleanPath(QDir::homePath());
}

} // namespace

QStringList removeOwnPaths(const QList<OwnPath> &paths)
{
    QStringList problems;
    for (const OwnPath &own : paths) {
        if (!isSafe(own.path)) {
            problems << u"Left %1 alone, as it isn't a place the app keeps for itself."_s.arg(own.what);
            continue;
        }
        const QFileInfo info(own.path);
        // a link is taken away and not followed
        if (!info.exists() && !info.isSymLink())
            continue;
        const bool gone =
            info.isDir() && !info.isSymLink() ? QDir(own.path).removeRecursively() : QFile::remove(own.path);
        if (!gone)
            problems << u"Couldn't remove %1, at %2."_s.arg(own.what, own.path);
    }
    return problems;
}

} // namespace pou
