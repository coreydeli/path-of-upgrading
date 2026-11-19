// SPDX-License-Identifier: MIT
#include "services/ownpaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// With forward slashes and no . or .. parts, or empty when it isn't an
// absolute path that starts with a slash or a drive letter and a slash.
QString absoluteClean(const QString &raw)
{
    static const QRegularExpression drive(uR"(^[A-Za-z]:[/\\])"_s);
    const QString slashed = QString(raw).replace(u'\\', u'/');
    // \\server\share and //server/share are left to nothing
    if (slashed.startsWith(u"//"_s))
        return {};
    if (!slashed.startsWith(u'/') && !drive.match(raw).hasMatch())
        return {};
    return QDir::cleanPath(slashed);
}

// The parts of a clean path, without its drive.
QStringList partsOf(const QString &clean)
{
    QStringList parts = clean.split(u'/', Qt::SkipEmptyParts);
    if (!parts.isEmpty() && parts.first().size() == 2 && parts.first().at(1) == u':')
        parts.removeFirst();
    return parts;
}

} // namespace

bool isOwnPath(const QString &path, const QString &marker)
{
    if (marker.isEmpty())
        return false;
    const QString clean = absoluteClean(path);
    if (clean.isEmpty())
        return false;
    const QStringList parts = partsOf(clean);
    if (parts.size() < 3 || !parts.last().contains(marker, Qt::CaseInsensitive))
        return false;
    // not home, and not a folder it's in
    const QString home = absoluteClean(QDir::homePath());
    return home.isEmpty() || !(home.compare(clean, Qt::CaseInsensitive) == 0 ||
                               home.startsWith(clean + u'/', Qt::CaseInsensitive));
}

QString ownFolderOf(const QString &path, const QString &marker)
{
    if (isOwnPath(path, marker))
        return path;
    const QString clean = absoluteClean(path);
    const QString up = clean.left(clean.lastIndexOf(u'/'));
    return isOwnPath(up, marker) ? up : path;
}

QStringList removeOwnPaths(const QList<OwnPath> &paths, const QString &marker)
{
    QStringList problems;
    for (const OwnPath &own : paths) {
        if (!isOwnPath(own.path, marker)) {
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
