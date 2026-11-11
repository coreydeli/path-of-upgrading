// SPDX-License-Identifier: MIT
#include "core/datafiles.h"
#include "core/pobguide.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// A Linux computer in a temporary folder, with flatpak and LuaJIT but no Path
// of Building yet.
struct Computer
{
    QTemporaryDir dir;
    PobPlaces places{
        .home = dir.filePath(u"home"_s),
        .dataHome = dir.filePath(u"home/.local/share"_s),
        .flatpakInstallations = {dir.filePath(u"home/.local/share/flatpak"_s),
                                 dir.filePath(u"var/lib/flatpak"_s)},
        .flatpak = u"/usr/bin/flatpak"_s,
        .luajit = u"/usr/bin/luajit"_s,
    };

    bool installFlatpak() const
    {
        return QDir().mkpath(places.flatpakInstallations.last() +
                             u"/app/community.pathofbuilding.PathOfBuilding/current/active"_s);
    }
    QString flatpakFolder() const
    {
        return places.home + u"/.var/app/community.pathofbuilding.PathOfBuilding/data/RustyPathOfBuilding2"_s;
    }
    QString dataFolder() const { return places.dataHome + u"/RustyPathOfBuilding2"_s; }
};

struct WindowsComputer
{
    QTemporaryDir dir;
    PobPlaces places{
        .home = dir.filePath(u"Users/Someone"_s),
        .dataHome = dir.filePath(u"Users/Someone/AppData/Local"_s),
        .luajit = u"C:/Program Files/Path of Upgrading/luajit.exe"_s,
        .appData = dir.filePath(u"Users/Someone/AppData/Roaming"_s),
        .documents = dir.filePath(u"Users/Someone/Documents"_s),
    };

    QString installFolder() const { return places.appData + u"/Path of Building Community (PoE2)"_s; }
};

bool addPob(const QString &folder, const QByteArray &manifest = {})
{
    return writeFile(folder + u"/HeadlessWrapper.lua"_s, {}) &&
           (manifest.isEmpty() || writeFile(folder + u"/manifest.xml"_s, manifest));
}

// What the page is given when the app found it
PobGuide guideFor(const PobPlaces &places, const QString &location = {}, const QString &runProblem = {})
{
    return guideToPob(places, location, findPob(places, location), runProblem);
}

QString text(const PobGuide &guide)
{
    return guide.headline + u'\n' + guide.details.join(u'\n') + u'\n' + guide.steps.join(u'\n');
}

const QByteArray poe1Manifest =
    "<PoBVersion><Source url=\"https://example.com/PathOfBuilding/{branch}/src/\"/>"
    "</PoBVersion>";

} // namespace

class TestPobGuide : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void links()
    {
        QCOMPARE(pobDiscoverUrl(), u"appstream://community.pathofbuilding.PathOfBuilding"_s);
        QCOMPARE(pobFlathubUrl(), u"https://flathub.org/apps/community.pathofbuilding.PathOfBuilding"_s);
        QCOMPARE(pobInstallCommand(), u"flatpak install flathub community.pathofbuilding.PathOfBuilding"_s);
        QCOMPARE(pobInstallerUrl(),
                 u"https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2/releases"_s);
    }

    void flatpak()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        const PobGuide guide = guideFor(computer.places);
        QCOMPARE(guide.headline, u"Using Path of Building from Flathub."_s);
        QCOMPARE(guide.installedAt, QDir::toNativeSeparators(computer.flatpakFolder()));
        QCOMPARE(guide.buildsAt, QDir::toNativeSeparators(computer.flatpakFolder() +
                                                          u"/userdata/Path of Building (PoE2)/Builds"_s));
        // for someone who has never heard of a Flatpak
        QVERIFY(guide.details.join(u' ').contains(u"hidden folder named .var"));
        QVERIFY(guide.steps.isEmpty());
        QVERIFY(!guide.getFlatpak);
        QVERIFY(!guide.folderProblem);
        QVERIFY(guide.folderNote.contains(u"Leave it empty to use the one found."));
        QVERIFY(guide.folderNote.contains(u"AUR"));
        QVERIFY(guide.folderNote.contains(u"HeadlessWrapper.lua"));
    }

    void noBuildsFolderToShow()
    {
        Computer computer;
        const PobGuide guide = guideToPob(computer.places, {}, PobInstall{.folder = u"/pob"_s}, {});
        QCOMPARE(guide.installedAt, QDir::toNativeSeparators(u"/pob"_s));
        QVERIFY(guide.buildsAt.isEmpty());
    }

    void inTheDataFolder()
    {
        Computer computer;
        QVERIFY(addPob(computer.dataFolder()));
        const PobGuide guide = guideFor(computer.places);
        QCOMPARE(guide.headline, u"Using the Path of Building in your data folder."_s);
        QCOMPARE(guide.installedAt, QDir::toNativeSeparators(computer.dataFolder()));
        QVERIFY(!guide.buildsAt.isEmpty());
        QVERIFY(guide.details.isEmpty());
    }

    void chosenFolder()
    {
        Computer computer;
        const QString folder = computer.dir.filePath(u"games/pob"_s);
        QVERIFY(addPob(folder));
        const PobGuide guide = guideFor(computer.places, folder);
        QCOMPARE(guide.headline, u"Using the Path of Building in the folder you chose."_s);
        QCOMPARE(guide.installedAt, QDir::toNativeSeparators(folder));
        QVERIFY(guide.folderNote.startsWith(u"Used instead of the usual places."));
        QVERIFY(!guide.folderProblem);
        QCOMPARE(guide.places.size(), 1);
        QVERIFY(guide.places.first().startsWith(u"The folder you chose: "));
        QVERIFY(guide.places.first().endsWith(u", found"));
    }

    void notInstalled()
    {
        Computer computer;
        const PobGuide guide = guideFor(computer.places);
        QCOMPARE(guide.headline, u"Path of Building wasn't found."_s);
        QVERIFY(guide.installedAt.isEmpty());
        QCOMPARE(guide.steps.size(), 3);
        QVERIFY(guide.steps[0].startsWith(u"1. Install \"Path of Building\" from Flathub, in Discover"));
        QVERIFY(guide.steps[1].startsWith(u"2. Open \"Path of Building Community (PoE2)\" once"));
        QCOMPARE(guide.steps[2], u"3. Press Update."_s);
        QVERIFY(guide.getFlatpak);
        QVERIFY(!guide.getInstaller);
    }

    void notInstalledWithoutFlatpak()
    {
        Computer computer;
        computer.places.flatpak.clear();
        const PobGuide guide = guideFor(computer.places);
        QCOMPARE(guide.steps.size(), 4);
        QVERIFY(guide.steps[0].contains(u"Install Flatpak"));
        QVERIFY(guide.steps[0].contains(u"https://flatpak.org/setup/"));
        QCOMPARE(guide.steps[3], u"4. Restart Path of Upgrading."_s);
        QVERIFY(guide.getFlatpak);
    }

    void notOpened()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        const PobGuide guide = guideFor(computer.places);
        QVERIFY(guide.headline.contains(u"not set up for Path of Exile 2"));
        QVERIFY(guide.steps[0].contains(u"Open \"Path of Building Community (PoE2)\""));
        QVERIFY(guide.steps.last().contains(u"Update"));
        // it's installed already
        QVERIFY(!guide.getFlatpak);
    }

    void withoutLuajit()
    {
        Computer computer;
        QVERIFY(addPob(computer.dataFolder()));
        computer.places.luajit.clear();
        const PobGuide guide = guideFor(computer.places);
        QCOMPARE(guide.headline, u"Path of Building was found, but LuaJIT is missing."_s);
        QCOMPARE(guide.installedAt, QDir::toNativeSeparators(computer.dataFolder()));
        QVERIFY(text(guide).contains(u"\"luajit\" package"));
        QVERIFY(guide.steps.last().contains(u"Restart Path of Upgrading"));
        QVERIFY(!guide.getFlatpak);
    }

    void chosenFolderProblems()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        const QString empty = computer.dir.filePath(u"empty"_s);
        QVERIFY(QDir().mkpath(empty));

        PobGuide guide = guideFor(computer.places, empty);
        QCOMPARE(guide.headline, u"There's no Path of Building in the folder you chose."_s);
        QVERIFY(guide.folderProblem);
        QVERIFY(guide.folderNote.contains(u"HeadlessWrapper.lua"));
        QVERIFY(text(guide).contains(u"the usual places have this: Using Path of Building from Flathub."));
        QVERIFY(guide.steps.last().contains(u"clear the folder"));
        QVERIFY(guide.places.first().endsWith(u", not there"));

        const QString poe1 = computer.dir.filePath(u"poe1"_s);
        QVERIFY(addPob(poe1, poe1Manifest));
        guide = guideFor(computer.places, poe1);
        QVERIFY(guide.headline.contains(u"Path of Exile 1"));
        QVERIFY(guide.folderProblem);
        QVERIFY(guide.folderNote.contains(u"PathOfBuilding-PoE2"));

        const QString plain = computer.dir.filePath(u"pob"_s);
        QVERIFY(addPob(plain));
        computer.places.luajit.clear();
        guide = guideFor(computer.places, plain);
        QVERIFY(guide.headline.contains(u"needs LuaJIT"));
        QVERIFY(guide.folderProblem);
        QVERIFY(guide.steps.first().contains(u"\"luajit\" package"));
    }

    void chosenFolderWithNothingElse()
    {
        Computer computer;
        const QString empty = computer.dir.filePath(u"empty"_s);
        QVERIFY(QDir().mkpath(empty));
        const PobGuide guide = guideFor(computer.places, empty);
        QVERIFY(text(guide).contains(u"Nothing is found in the usual places either."));
    }

    void wouldNotStart()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        const PobGuide guide = guideFor(computer.places, {}, u"Path of Building didn't start in time."_s);
        QCOMPARE(guide.headline, u"Path of Building was found, but it wouldn't start."_s);
        QVERIFY(guide.details.contains(u"Path of Building didn't start in time."_s));
        QCOMPARE(guide.steps.size(), 2);
        QVERIFY(guide.steps[1].contains(u"open Path of Building Community (PoE2) yourself"));
        // it's still where it was found
        QCOMPARE(guide.installedAt, QDir::toNativeSeparators(computer.flatpakFolder()));
    }

    void turnedUpSinceLooking()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        const PobGuide guide = guideToPob(computer.places, {}, std::nullopt, {});
        QCOMPARE(guide.headline, u"Path of Building is there now."_s);
        QCOMPARE(guide.steps, QStringList{u"1. Press Update."_s});
    }

    void placesItLooks()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.dataFolder()));
        computer.places.luajit.clear();
        const QStringList places = guideFor(computer.places).places;
        const auto line = [&](const QString &start) {
            for (const QString &place : places) {
                if (place.startsWith(start))
                    return place;
            }
            return QString();
        };
        QCOMPARE(line(u"The Flatpak, installed for you: "_s),
                 u"The Flatpak, installed for you: %1, not there"_s.arg(
                     QDir::toNativeSeparators(computer.places.flatpakInstallations[0] +
                                              u"/app/community.pathofbuilding.PathOfBuilding"_s)));
        QVERIFY(line(u"The Flatpak, installed for everyone: "_s).endsWith(u", found"));
        QVERIFY(line(u"The Flatpak's files for Path of Exile 2: "_s).endsWith(u", not there"));
        QVERIFY(line(u"Not from the Flatpak: "_s).endsWith(u", found"));
        QCOMPARE(line(u"The flatpak command: "_s),
                 u"The flatpak command: %1, found"_s.arg(QDir::toNativeSeparators(u"/usr/bin/flatpak"_s)));
        QCOMPARE(line(u"LuaJIT: "_s), u"LuaJIT: not found"_s);
    }

    void windows()
    {
        WindowsComputer computer;
        QVERIFY(addPob(computer.installFolder()));
        QVERIFY(writeFile(computer.installFolder() + u"/installed.cfg"_s, {}));
        PobGuide guide = guideFor(computer.places);
        QCOMPARE(guide.headline, u"Using Path of Building Community (PoE2), from its installer."_s);
        QCOMPARE(guide.installedAt, QDir::toNativeSeparators(computer.installFolder()));
        QVERIFY(guide.buildsAt.contains(u"Path of Building (PoE2)"));
        QVERIFY(guide.folderNote.contains(u"unzipped"));
        QVERIFY(!guide.folderNote.contains(u"AUR"));
        QCOMPARE(guide.places.size(), 1);
        QVERIFY(guide.places.first().startsWith(u"Where its installer puts it: "));

        computer.places.luajit.clear();
        guide = guideFor(computer.places);
        QCOMPARE(guide.headline, u"Path of Building was found, but LuaJIT is missing."_s);
        QVERIFY(text(guide).contains(u"Install Path of Upgrading again"));
        QVERIFY(!guide.getInstaller);
    }

    void windowsNotInstalled()
    {
        WindowsComputer computer;
        const PobGuide guide = guideFor(computer.places);
        QCOMPARE(guide.headline, u"Path of Building wasn't found."_s);
        QVERIFY(guide.getInstaller);
        QVERIFY(!guide.getFlatpak);
        QVERIFY(text(guide).contains(u"Run the installer."));
        QVERIFY(!text(guide).contains(u"Flatpak"));
    }

    void windowsWouldNotStart()
    {
        WindowsComputer computer;
        QVERIFY(addPob(computer.installFolder()));
        const PobGuide guide = guideFor(computer.places, {}, u"Couldn't run Path of Building."_s);
        QVERIFY(guide.steps[1].contains(u"open Path of Building yourself"));
    }
};

QTEST_GUILESS_MAIN(TestPobGuide)
#include "tst_pobguide.moc"
