// SPDX-License-Identifier: MIT
#include "core/pobguide.h"

#include <QDir>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QString shown(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

bool onWindows(const PobPlaces &places)
{
    return !places.appData.isEmpty();
}

// "1. Open it." and so on
QStringList numbered(const QStringList &steps)
{
    QStringList lines;
    for (qsizetype i = 0; i < steps.size(); ++i)
        lines << u"%1. %2"_s.arg(i + 1).arg(steps[i]);
    return lines;
}

QString whichIsUsed(const PobPlaces &places, const QString &location, const PobInstall &install)
{
    if (!location.isEmpty())
        return u"Using the Path of Building in the folder you chose."_s;
    if (install.flatpak)
        return u"Using Path of Building from Flathub."_s;
    if (onWindows(places))
        return u"Using Path of Building Community (PoE2), from its installer."_s;
    return u"Using the Path of Building in your data folder."_s;
}

QString folderNoteFor(const PobPlaces &places)
{
    if (onWindows(places)) {
        return u"Only for a Path of Building you unzipped instead of using its installer. Pick the folder "
               u"with HeadlessWrapper.lua in it, or the folder of its repository. Leave it empty to use "
               u"the one found."_s;
    }
    return u"Only for a Path of Building you installed some other way, such as from its source, the AUR or "
           u"a zip. Pick the folder with HeadlessWrapper.lua in it, or the folder of its repository. "
           u"Leave it empty to use the one found."_s;
}

QString placeLine(const PobPlace &place)
{
    if (place.path.isEmpty())
        return u"%1: not found"_s.arg(place.label);
    return u"%1: %2, %3"_s.arg(place.label, shown(place.path), place.present ? u"found"_s : u"not there"_s);
}

// What the usual places have, for when the folder set is no good.
QString usualPlaces(const PobPlaces &places, const std::optional<PobInstall> &usual)
{
    if (!usual)
        return u"Nothing is found in the usual places either."_s;
    return u"Without it, the usual places have this: %1"_s.arg(whichIsUsed(places, {}, *usual));
}

} // namespace

QString pobDiscoverUrl()
{
    return u"appstream://"_s + pobFlatpakId;
}

QString pobFlathubUrl()
{
    return u"https://flathub.org/apps/"_s + pobFlatpakId;
}

QString pobInstallCommand()
{
    return u"flatpak install flathub "_s + pobFlatpakId;
}

QString pobInstallerUrl()
{
    return u"https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2/releases"_s;
}

PobGuide guideToPob(const PobPlaces &places, const QString &location,
                    const std::optional<PobInstall> &running, const QString &runProblem)
{
    PobGuide guide;
    guide.folderNote = folderNoteFor(places);
    for (const PobPlace &place : searchedPlaces(places, location))
        guide.places << placeLine(place);

    if (running) {
        guide.headline = whichIsUsed(places, location, *running);
        guide.installedAt = shown(running->folder);
        if (!running->userFolder.isEmpty())
            guide.buildsAt = shown(pobBuildsFolder(*running));
        if (running->flatpak) {
            guide.details << u"It's a Flatpak, which keeps its files in a hidden folder named .var in your "
                             u"home folder."_s;
        }
        if (!location.isEmpty())
            guide.folderNote = u"Used instead of the usual places. Clear it to go back to them."_s;
        if (!runProblem.isEmpty()) {
            guide.headline = u"Path of Building was found, but it wouldn't start."_s;
            guide.details << runProblem;
            guide.steps << u"Press Update to try once more."_s
                        << u"If it keeps failing, open %1 yourself to see whether it works."_s.arg(
                               onWindows(places) ? u"Path of Building"_s : u"Path of Building PoE 2"_s);
        }
        guide.steps = numbered(guide.steps);
        return guide;
    }

    const PobLookup lookup = lookForPob(places, location);
    if (lookup.install) {
        // it turned up since the app looked
        guide.headline = u"Path of Building is there now."_s;
        guide.steps = numbered({u"Press Update."_s});
        return guide;
    }

    const bool noFlatpak = places.flatpak.isEmpty();
    switch (lookup.problem) {
    case PobProblem::NotInstalled:
        guide.headline = u"Path of Building wasn't found."_s;
        guide.details << u"Path of Upgrading uses the Path of Building from Flathub to work out how an item "
                         u"changes your build."_s;
        if (noFlatpak)
            guide.steps << u"Install Flatpak on this computer, see https://flatpak.org/setup/"_s;
        guide.steps << u"Install \"Path of Building\" from Flathub, in Discover or with the buttons below."_s
                    << u"Open \"Path of Building PoE 2\" once, and wait while it downloads what it needs."_s
                    << (noFlatpak ? u"Restart Path of Upgrading."_s : u"Press Update."_s);
        guide.getFlatpak = true;
        break;
    case PobProblem::NotOpened:
        guide.headline = u"Path of Building is installed, but not set up for Path of Exile 2 yet."_s;
        guide.details << u"It downloads what it needs for Path of Exile 2 the first time you open it for "
                         u"that game."_s;
        guide.steps << u"Open \"Path of Building PoE 2\" from your application menu."_s
                    << u"Wait until its window shows."_s << u"Press Update."_s;
        break;
    case PobProblem::NoLuajit:
        guide.headline = u"Path of Building was found, but LuaJIT is missing."_s;
        guide.installedAt = shown(lookup.folder);
        guide.details << u"LuaJIT is what runs Path of Building without its window."_s;
        guide.steps << u"Install the \"luajit\" package with your package manager."_s
                    << u"Restart Path of Upgrading."_s;
        break;
    case PobProblem::NotInstalledOnWindows:
        guide.headline = u"Path of Building wasn't found."_s;
        guide.details << u"Path of Upgrading uses Path of Building Community (PoE2) from its installer."_s;
        guide.steps << u"Download its installer from the releases page, with the button below."_s
                    << u"Run the installer."_s << u"Press Update."_s;
        guide.getInstaller = true;
        break;
    case PobProblem::NoLuajitOnWindows:
        guide.headline = u"Path of Building was found, but LuaJIT is missing."_s;
        guide.details << u"LuaJIT comes with Path of Upgrading, and should be in the folder it is installed "
                         u"in."_s;
        guide.steps << u"Install Path of Upgrading again to put it back."_s << u"Start it again."_s;
        break;
    case PobProblem::NothingInFolder:
    case PobProblem::ForPoe1:
    case PobProblem::FolderNoLuajit:
        guide.folderProblem = true;
        guide.details << usualPlaces(places, findPob(places, {}));
        if (lookup.problem == PobProblem::NothingInFolder) {
            guide.headline = u"There's no Path of Building in the folder you chose."_s;
            guide.folderNote = u"Pick the folder with HeadlessWrapper.lua in it, or the folder of its "
                               u"repository."_s;
        } else if (lookup.problem == PobProblem::ForPoe1) {
            guide.headline = u"The folder you chose has Path of Building for Path of Exile 1."_s;
            guide.folderNote =
                u"Pick the one for Path of Exile 2, from the PathOfBuilding-PoE2 repository."_s;
        } else {
            guide.headline = u"The Path of Building in the folder you chose needs LuaJIT."_s;
            guide.folderNote = u"LuaJIT runs Path of Building without its window, and it isn't installed."_s;
        }
        guide.steps << (lookup.problem == PobProblem::FolderNoLuajit
                            ? u"Install the \"luajit\" package, and restart Path of Upgrading."_s
                            : u"Pick the right folder."_s)
                    << u"Or clear the folder, to use the usual places."_s;
        break;
    }
    guide.steps = numbered(guide.steps);
    return guide;
}

} // namespace pou
