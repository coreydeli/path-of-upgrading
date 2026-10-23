// SPDX-License-Identifier: MIT
#include "core/pobinstall.h"

#include <QDir>
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
    };
}

std::optional<PobInstall> findPob(const PobPlaces &places, QString *why)
{
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
        };
    }

    if (flatpak)
        setWhy(why, u"Open Path of Building PoE 2 once, so it downloads what it needs."_s);
    else
        setWhy(why, u"Path of Building isn't installed. Get it from Flathub with: "
                    u"flatpak install flathub %1"_s.arg(pobFlatpakId));
    return std::nullopt;
}

} // namespace pou
