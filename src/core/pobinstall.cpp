// SPDX-License-Identifier: MIT
#include "core/pobinstall.h"

#include "core/datafiles.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Where rusty-path-of-building keeps Path of Building for Path of Exile 2,
// in its data folder.
constexpr QStringView poe2Folder = u"/RustyPathOfBuilding2";
// and where that keeps what's yours, next to it
constexpr QStringView userFolder = u"/userdata/Path of Building (PoE2)";

void setWhy(QString *why, const QString &text)
{
    if (why)
        *why = text;
}

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

std::optional<PobInstall> pobIn(const PobPlaces &places, const QString &location, QString *why)
{
    QString folder = QDir::cleanPath(location);
    if (!hasPob(folder))
        folder += u"/src"_s; // a copy of its repository
    if (!hasPob(folder)) {
        setWhy(why, u"There's no Path of Building in %1."_s.arg(location));
        return std::nullopt;
    }
    if (isForPoe1(folder)) {
        setWhy(why, u"The Path of Building in %1 is for Path of Exile 1."_s.arg(location));
        return std::nullopt;
    }
    if (places.luajit.isEmpty()) {
        setWhy(why,
               u"Running the Path of Building in %1 needs LuaJIT, which isn't installed."_s.arg(location));
        return std::nullopt;
    }
    return PobInstall{.folder = folder, .program = places.luajit};
}

// The builds folder chosen in Path of Building's options. Nothing else in
// its settings is read, as they have your sign in to the trade site too.
QString chosenBuildsFolder(const QString &settings)
{
    const std::optional<QByteArray> xml = readFile(settings);
    if (!xml)
        return {};
    QXmlStreamReader reader(*xml);
    if (!reader.readNextStartElement())
        return {};
    while (reader.readNextStartElement()) {
        if (reader.name() == u"Misc")
            return reader.attributes().value(u"buildPath").toString();
        reader.skipCurrentElement();
    }
    return {};
}

} // namespace

PobPlaces pobPlaces()
{
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return {
        .home = QDir::homePath(),
        .dataHome = dataHome,
        // where flatpak itself looks
        .flatpakInstallations = {qEnvironmentVariable("FLATPAK_USER_DIR", dataHome + u"/flatpak"_s),
                                 qEnvironmentVariable("FLATPAK_SYSTEM_DIR", u"/var/lib/flatpak"_s)},
        .flatpak = QStandardPaths::findExecutable(u"flatpak"_s),
        .luajit = QStandardPaths::findExecutable(u"luajit"_s),
    };
}

std::optional<PobInstall> findPob(const PobPlaces &places, const QString &location, QString *why)
{
    if (!location.isEmpty())
        return pobIn(places, location, why);

    const bool flatpak = hasFlatpak(places);
    // in its own data folder, which is $XDG_DATA_HOME in the sandbox
    const QString sandboxed = places.home + u"/.var/app/"_s + pobFlatpakId + u"/data"_s + poe2Folder;
    if (flatpak && hasPob(sandboxed)) {
        return PobInstall{
            .folder = sandboxed,
            .program = places.flatpak,
            // stopped along with the app, and without the network, which
            // nothing the app asks of it needs
            .arguments = {u"run"_s, u"--die-with-parent"_s, u"--unshare=network"_s, u"--command=luajit"_s,
                          pobFlatpakId.toString()},
            .flatpak = true,
        };
    }
    const QString native = places.dataHome + poe2Folder;
    if (hasPob(native) && !places.luajit.isEmpty())
        return PobInstall{.folder = native, .program = places.luajit};

    if (flatpak)
        setWhy(why, u"Open Path of Building PoE 2 once, so it downloads what it needs."_s);
    else if (hasPob(native))
        setWhy(why, u"Running Path of Building needs LuaJIT, which isn't installed."_s);
    else
        setWhy(why, u"Path of Building isn't installed. Get it from Flathub with: "
                    u"flatpak install flathub %1"_s.arg(pobFlatpakId));
    return std::nullopt;
}

QString pobBuildsFolder(const PobInstall &install)
{
    const QString user = install.folder + userFolder;
    // one chosen in the Flatpak's sandbox can be a folder only it has
    if (const QString chosen = chosenBuildsFolder(user + u"/Settings.xml"_s);
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

} // namespace pou
