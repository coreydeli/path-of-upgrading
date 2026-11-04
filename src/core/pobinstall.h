// SPDX-License-Identifier: MIT
#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <optional>

namespace pou {

// Path of Building for Path of Exile 2, which the app runs without its
// window to judge items with your build. On Linux it's normally the Flatpak
// from Flathub, rusty-path-of-building, which downloads Path of Building the
// first time you open it for Path of Exile 2. The app runs that with the
// Flatpak's own LuaJIT. On Windows it's the one from its installer, run with
// the LuaJIT that comes with the app.

inline constexpr QStringView pobFlatpakId = u"community.pathofbuilding.PathOfBuilding";

// How to run the Path of Building that was found: the program with the
// arguments, then LuaJIT's, in the folder.
struct PobInstall
{
    QString folder; // the one with HeadlessWrapper.lua
    QString program;
    QStringList arguments;
    bool flatpak = false; // in the Flatpak's sandbox
    QString userFolder;   // where it keeps your settings and builds

    friend bool operator==(const PobInstall &, const PobInstall &) = default;
};

// Where to look for it.
struct PobPlaces
{
    QString home;
    QString dataHome;                 // $XDG_DATA_HOME
    QStringList flatpakInstallations; // yours, then the system's
    QString flatpak;                  // the flatpak command, empty when it isn't installed
    QString luajit;                   // the same for LuaJIT
    // Only on Windows: %APPDATA%, where its installer puts it, and your
    // Documents, where that keeps what's yours.
    QString appData;
    QString documents;
};

// The places on this computer.
PobPlaces pobPlaces();

// Why there's none to run.
enum class PobProblem {
    NotInstalled,          // no Flatpak, and nothing in the data folder
    NotOpened,             // the Flatpak is there, but hasn't downloaded Path of Building for Path of Exile 2
    NoLuajit,              // it's in the data folder, with nothing to run it
    NotInstalledOnWindows, // not where its installer puts it
    NoLuajitOnWindows,
    NothingInFolder, // in the folder you set
    ForPoe1,         // in the folder you set
    FolderNoLuajit,  // in the folder you set
};

// What looking for Path of Building found.
struct PobLookup
{
    std::optional<PobInstall> install;
    PobProblem problem = PobProblem::NotInstalled; // when there's no install
    QString why;                                   // in words for people, the same
};

// The Path of Building in the location when that's set, or else the
// Flatpak's, or else one from rusty-path-of-building installed some other
// way. On Windows it's the installed one.
PobLookup lookForPob(const PobPlaces &places, const QString &location);

// The same, as only the install: empty when there's none, with why in
// words for people when it's given.
std::optional<PobInstall> findPob(const PobPlaces &places, const QString &location, QString *why = nullptr);

// Where it keeps the builds you save in it: the folder chosen in its
// options, or else its own.
QString pobBuildsFolder(const PobInstall &install);

// A build you saved in Path of Building.
struct SavedBuild
{
    QString path;
    QString name; // as it lists it, after the folders it's in: "Leveling/Witch"
    QDateTime modified;

    friend bool operator==(const SavedBuild &, const SavedBuild &) = default;
};

// The builds in the folder and the folders in it, the last one saved first.
QList<SavedBuild> savedBuilds(const QString &folder);

} // namespace pou
