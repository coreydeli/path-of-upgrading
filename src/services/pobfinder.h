// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobguide.h"
#include "core/pobinstall.h"

#include <QList>
#include <QString>
#include <optional>

namespace pou {

// Looking for Path of Building on this computer, see core/pobinstall.h.

// The places on this computer.
PobPlaces pobPlaces();

// The Path of Building in the location when that's set, or else the
// Flatpak's, or else one from rusty-path-of-building installed some other
// way. On Windows it's the installed one.
PobLookup lookForPob(const PobPlaces &places, const QString &location);

// The same, as only the install: empty when there's none, with why in
// words for people when it's given.
std::optional<PobInstall> findPob(const PobPlaces &places, const QString &location, QString *why = nullptr);

// The places that findPob() looks in for the location, in the order it looks.
QList<PobPlace> searchedPlaces(const PobPlaces &places, const QString &location);

// Where it keeps the builds you save in it: the folder chosen in its
// options, or else its own.
QString pobBuildsFolder(const PobInstall &install);

// The builds in the folder and the folders in it, the last one saved first.
QList<SavedBuild> savedBuilds(const QString &folder);

// What the guide on the Build Review page needs to know of the places, with
// the Path of Building that's running.
PobSurvey surveyPob(const PobPlaces &places, const QString &location,
                    const std::optional<PobInstall> &running);

} // namespace pou
