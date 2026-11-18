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

// Removes each, with all that's in a folder. A path that isn't there is
// already gone, and one that can't be removed doesn't stop the rest. Gives
// back a sentence for each that couldn't be, including any that is empty, the
// root or your home folder, which are never the app's own.
QStringList removeOwnPaths(const QList<OwnPath> &paths);

} // namespace pou
