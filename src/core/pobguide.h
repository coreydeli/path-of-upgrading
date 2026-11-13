// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobinstall.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// Where to get Path of Building for Path of Exile 2 on Linux: Discover's page
// for it, Flathub's page, and the command that installs it.
QString pobDiscoverUrl();
QString pobFlathubUrl();
QString pobInstallCommand();
// and on Windows, the releases page with its installer
QString pobInstallerUrl();

// What the Build Review page says about where Path of Building is: what
// was found and where, or what's missing and what to do about it.
struct PobGuide
{
    QString headline;           // what was found, or that nothing was
    QStringList details;        // what's wrong or worth knowing, a sentence each
    QStringList steps;          // what to do, numbered
    QString installedAt;        // the folder it runs from, empty when there's none
    QString buildsAt;           // and where its builds are saved
    bool getFlatpak = false;    // offer Discover, Flathub and the install command
    bool getInstaller = false;  // offer the page with Windows' installer
    QString folderNote;         // under the folder field
    bool folderProblem = false; // that note is about what's wrong with the folder
    QStringList places;         // where it looks, one place a line
};

// What looking in the places found, which surveyPob() in
// services/pobfinder.h gathers.
struct PobSurvey
{
    QList<PobPlace> places; // each place it looks in, in order
    // Only when nothing's running: what's there, and what the usual places
    // have when the folder set is no good.
    PobLookup lookup;
    std::optional<PobInstall> usual;
    QString buildsFolder; // where the one running keeps its builds
};

// The guide for the folder set (empty for the usual places), with the Path of
// Building that's running, and why it couldn't start when that's so.
PobGuide guideToPob(const PobPlaces &places, const QString &location,
                    const std::optional<PobInstall> &running, const QString &runProblem,
                    const PobSurvey &survey);

} // namespace pou
