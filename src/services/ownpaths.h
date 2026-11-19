// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace pou {

// A file or a folder the app keeps for itself.
struct OwnPath
{
    QString what; // for people, "your settings"
    QString path;
};

// Whether a path is one the app may remove: an absolute path, on any system
// (a drive letter or a leading slash), that isn't a drive's root or the top
// of a share, whose last part has the marker in its name (the app's own name,
// as in .../path-of-upgrading or .../path-of-upgradingrc), and that isn't
// your home folder or a folder above it. It only reads the text, so it can be
// tried with made-up paths.
bool isOwnPath(const QString &path, const QString &marker);

// The path when isOwnPath says it's the app's, or else the folder it's in when
// that one is, or else the path as it is (which isOwnPath then refuses). Qt
// puts the cache on Windows in a "cache" folder inside the app's own,
// AppData/Local/path-of-upgrading/cache, so that's the folder to remove.
QString ownFolderOf(const QString &path, const QString &marker);

// Removes each, with all that's in a folder, once isOwnPath says it's the
// app's. A path that isn't there is already gone, and one that can't be
// removed doesn't stop the rest. Gives back a sentence for each that was left
// alone or couldn't be removed.
QStringList removeOwnPaths(const QList<OwnPath> &paths, const QString &marker);

} // namespace pou
