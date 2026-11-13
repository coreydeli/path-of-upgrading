// SPDX-License-Identifier: MIT
#include "services/pobfinder.h"

#include "core/datafiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Where rusty-path-of-building keeps Path of Building for Path of Exile 2,
// in its data folder.
constexpr QStringView poe2Folder = u"/RustyPathOfBuilding2";
// and where that keeps what's yours, next to it
constexpr QStringView userFolder = u"/userdata/Path of Building (PoE2)";
// Where its installer puts it on Windows, in %APPDATA%
constexpr QStringView windowsFolder = u"/Path of Building Community (PoE2)";
// and where that keeps what's yours, in your Documents
constexpr QStringView documentsFolder = u"/Path of Building (PoE2)";

bool hasPob(const QString &folder)
{
    return QFile::exists(folder + u"/HeadlessWrapper.lua"_s);
}

bool hasFlatpak(const PobPlaces &places)
{
    return !places.flatpak.isEmpty() &&
           std::any_of(places.flatpakInstallations.begin(), places.flatpakInstallations.end(),
                       [](const QString &installation) {
                           return QFileInfo::exists(installation + u"/app/"_s + pobFlatpakId +
                                                    u"/current/active"_s);
                       });
}

// Path of Building for Path of Exile 1 has the same files, but its manifest
// updates it from that game's repository. The manifest is next to the files,
// or a folder up in a copy of the repository.
bool isForPoe1(const QString &folder)
{
    const QString manifests[] = {folder + u"/manifest.xml"_s, folder + u"/../manifest.xml"_s};
    for (const QString &manifest : manifests) {
        if (const std::optional<QByteArray> text = readFile(manifest))
            return !text->contains("PathOfBuilding-PoE2");
    }
    return false;
}

// rusty-path-of-building keeps what's yours next to Path of Building. On
// Windows the installed one keeps it in your Documents, and one unzipped
// anywhere else keeps it in its own folder.
QString userFolderOf(const PobPlaces &places, const QString &folder)
{
    if (places.appData.isEmpty())
        return folder + userFolder;
    if (QFile::exists(folder + u"/installed.cfg"_s))
        return places.documents + documentsFolder;
    return folder;
}

PobLookup notFound(PobProblem problem, const QString &why)
{
    return {.problem = problem, .why = why};
}

PobLookup pobIn(const PobPlaces &places, const QString &location)
{
    QString folder = QDir::cleanPath(location);
    if (!hasPob(folder))
        folder += u"/src"_s; // a copy of its repository
    if (!hasPob(folder))
        return notFound(PobProblem::NothingInFolder, u"There's no Path of Building in %1."_s.arg(location));
    if (isForPoe1(folder)) {
        return notFound(PobProblem::ForPoe1,
                        u"The Path of Building in %1 is for Path of Exile 1."_s.arg(location));
    }
    if (places.luajit.isEmpty()) {
        return notFound(
            PobProblem::FolderNoLuajit,
            u"Running the Path of Building in %1 needs LuaJIT, which isn't installed."_s.arg(location));
    }
    return {.install = PobInstall{
                .folder = folder, .program = places.luajit, .userFolder = userFolderOf(places, folder)}};
}

PobLookup installedOnWindows(const PobPlaces &places)
{
    const QString folder = places.appData + windowsFolder;
    if (!hasPob(folder)) {
        return notFound(PobProblem::NotInstalledOnWindows,
                        u"Path of Building isn't installed. Get Path of Building Community (PoE2) from: "
                        u"https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2/releases"_s);
    }
    if (places.luajit.isEmpty()) {
        return notFound(
            PobProblem::NoLuajitOnWindows,
            u"LuaJIT is missing from Path of Upgrading's folder. Installing it again puts it back."_s);
    }
    return {.install = PobInstall{
                .folder = folder, .program = places.luajit, .userFolder = userFolderOf(places, folder)}};
}

} // namespace

PobPlaces pobPlaces()
{
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    PobPlaces places{
        .home = QDir::homePath(),
        .dataHome = dataHome,
        // where flatpak itself looks
        .flatpakInstallations = {qEnvironmentVariable("FLATPAK_USER_DIR", dataHome + u"/flatpak"_s),
                                 qEnvironmentVariable("FLATPAK_SYSTEM_DIR", u"/var/lib/flatpak"_s)},
        .flatpak = QStandardPaths::findExecutable(u"flatpak"_s),
        .luajit = QStandardPaths::findExecutable(u"luajit"_s),
    };
#ifdef Q_OS_WIN
    // the one that comes with the app, next to it
    if (const QString bundled =
            QStandardPaths::findExecutable(u"luajit"_s, {QCoreApplication::applicationDirPath()});
        !bundled.isEmpty())
        places.luajit = bundled;
    places.appData = QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA"));
    places.documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    return places;
}

PobLookup lookForPob(const PobPlaces &places, const QString &location)
{
    if (!location.isEmpty())
        return pobIn(places, location);
    if (!places.appData.isEmpty())
        return installedOnWindows(places);

    const bool flatpak = hasFlatpak(places);
    // in its own data folder, which is $XDG_DATA_HOME in the sandbox
    const QString sandboxed = places.home + u"/.var/app/"_s + pobFlatpakId + u"/data"_s + poe2Folder;
    if (flatpak && hasPob(sandboxed)) {
        return {.install = PobInstall{
                    .folder = sandboxed,
                    .program = places.flatpak,
                    // stopped along with the app, and without the network, which
                    // nothing the app asks of it needs
                    .arguments = {u"run"_s, u"--die-with-parent"_s, u"--unshare=network"_s,
                                  u"--command=luajit"_s, pobFlatpakId.toString()},
                    .flatpak = true,
                    .userFolder = sandboxed + userFolder,
                }};
    }
    const QString native = places.dataHome + poe2Folder;
    if (hasPob(native) && !places.luajit.isEmpty()) {
        return {.install = PobInstall{
                    .folder = native, .program = places.luajit, .userFolder = native + userFolder}};
    }

    if (flatpak) {
        return notFound(PobProblem::NotOpened,
                        u"Open Path of Building Community (PoE2) once, so it downloads what it needs."_s);
    }
    if (hasPob(native)) {
        PobLookup lookup = notFound(PobProblem::NoLuajit,
                                    u"Running Path of Building needs LuaJIT, which isn't installed."_s);
        lookup.folder = native;
        return lookup;
    }
    return notFound(PobProblem::NotInstalled, u"Path of Building isn't installed. Get it from Flathub with: "
                                              u"flatpak install flathub %1"_s.arg(pobFlatpakId));
}

QList<PobPlace> searchedPlaces(const PobPlaces &places, const QString &location)
{
    if (!location.isEmpty()) {
        const QString folder = QDir::cleanPath(location);
        return {{u"The folder you chose"_s, folder, hasPob(folder) || hasPob(folder + u"/src"_s)}};
    }
    if (!places.appData.isEmpty()) {
        const QString folder = places.appData + windowsFolder;
        return {{u"Where its installer puts it"_s, folder, hasPob(folder)}};
    }

    QList<PobPlace> list;
    for (qsizetype i = 0; i < places.flatpakInstallations.size(); ++i) {
        const QString &installation = places.flatpakInstallations[i];
        const QString app = installation + u"/app/"_s + pobFlatpakId;
        list << PobPlace{i == 0 ? u"The Flatpak, installed for you"_s
                                : u"The Flatpak, installed for everyone"_s,
                         app, QFileInfo::exists(app + u"/current/active"_s)};
    }
    const QString sandboxed = places.home + u"/.var/app/"_s + pobFlatpakId + u"/data"_s + poe2Folder;
    const QString native = places.dataHome + poe2Folder;
    list << PobPlace{u"The Flatpak's files for Path of Exile 2"_s, sandboxed, hasPob(sandboxed)}
         << PobPlace{u"Not from the Flatpak"_s, native, hasPob(native)}
         << PobPlace{u"The flatpak command"_s, places.flatpak, !places.flatpak.isEmpty()}
         << PobPlace{u"LuaJIT"_s, places.luajit, !places.luajit.isEmpty()};
    return list;
}

std::optional<PobInstall> findPob(const PobPlaces &places, const QString &location, QString *why)
{
    PobLookup lookup = lookForPob(places, location);
    if (!lookup.install && why)
        *why = lookup.why;
    return lookup.install;
}

QString pobBuildsFolder(const PobInstall &install)
{
    const QString &user = install.userFolder;
    // one chosen in the Flatpak's sandbox can be a folder only it has
    const std::optional<QByteArray> settings = readFile(user + u"/Settings.xml"_s);
    if (const QString chosen = settings ? chosenBuildsFolder(*settings) : QString();
        !chosen.isEmpty() && QFileInfo(chosen).isDir())
        return QDir::cleanPath(chosen);
    return user + u"/Builds"_s;
}

QList<SavedBuild> savedBuilds(const QString &folder)
{
    QList<SavedBuild> builds;
    const QDir dir(folder);
    QDirIterator it(folder, {u"*.xml"_s}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo file = it.nextFileInfo();
        QString name = dir.relativeFilePath(file.filePath());
        name.chop(4);
        builds << SavedBuild{.path = file.filePath(), .name = name, .modified = file.lastModified()};
    }
    std::sort(builds.begin(), builds.end(), [](const SavedBuild &a, const SavedBuild &b) {
        return a.modified != b.modified ? a.modified > b.modified : a.name < b.name;
    });
    return builds;
}

PobSurvey surveyPob(const PobPlaces &places, const QString &location,
                    const std::optional<PobInstall> &running)
{
    PobSurvey survey{.places = searchedPlaces(places, location)};
    if (running) {
        if (!running->userFolder.isEmpty())
            survey.buildsFolder = pobBuildsFolder(*running);
        return survey;
    }
    survey.lookup = lookForPob(places, location);
    // what's there without the folder set, when that's no good
    if (!survey.lookup.install && !location.isEmpty())
        survey.usual = findPob(places, {});
    return survey;
}

} // namespace pou
