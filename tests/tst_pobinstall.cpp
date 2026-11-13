// SPDX-License-Identifier: MIT
#include "core/datafiles.h"
#include "services/pobfinder.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// The part of a manifest that tells the games apart.
QByteArray manifest(const QByteArray &repository)
{
    return "<PoBVersion><Source part=\"program\" "
           "url=\"https://raw.githubusercontent.com/PathOfBuildingCommunity/" +
           repository + "/{branch}/src/\"/></PoBVersion>";
}

const QByteArray poe2Manifest = manifest("PathOfBuilding-PoE2");
const QByteArray poe1Manifest = manifest("PathOfBuilding");

const QStringList flatpakArguments = {u"run"_s, u"--die-with-parent"_s, u"--unshare=network"_s,
                                      u"--command=luajit"_s, u"community.pathofbuilding.PathOfBuilding"_s};

// A computer in a temporary folder, with flatpak and LuaJIT but no Path of
// Building yet.
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

    // what flatpak install leaves, as far as finding it goes: for everyone,
    // or for you with --user
    bool installFlatpak(bool user = false) const
    {
        return QDir().mkpath(places.flatpakInstallations.value(user ? 0 : 1) +
                             u"/app/community.pathofbuilding.PathOfBuilding/current/active"_s);
    }
    // where it downloads Path of Building for Path of Exile 2
    QString flatpakFolder() const
    {
        return places.home + u"/.var/app/community.pathofbuilding.PathOfBuilding/data/RustyPathOfBuilding2"_s;
    }
};

// Windows, in a temporary folder, with the LuaJIT that comes with the app.
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

    // where its installer puts it
    QString installFolder() const { return places.appData + u"/Path of Building Community (PoE2)"_s; }
};

// Path of Building's files, as far as finding it goes, with the manifest
// when it's given.
bool addPob(const QString &folder, const QByteArray &manifest = {})
{
    return writeFile(folder + u"/HeadlessWrapper.lua"_s, {}) &&
           (manifest.isEmpty() || writeFile(folder + u"/manifest.xml"_s, manifest));
}

} // namespace

class TestPobInstall : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void flatpak()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        QString why;
        const std::optional<PobInstall> pob = findPob(computer.places, {}, &why);
        QVERIFY2(pob, qPrintable(why));
        QCOMPARE(pob->folder, computer.flatpakFolder());
        QCOMPARE(pob->program, u"/usr/bin/flatpak"_s);
        QCOMPARE(pob->arguments, flatpakArguments);
        QVERIFY(pob->flatpak);
        QCOMPARE(pob->userFolder, computer.flatpakFolder() + u"/userdata/Path of Building (PoE2)"_s);
    }

    void flatpakForYou()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak(true));
        QVERIFY(addPob(computer.flatpakFolder()));
        QVERIFY(findPob(computer.places, {}));
    }

    void flatpakNotOpened()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        // only for Path of Exile 1
        QVERIFY(addPob(computer.places.home +
                       u"/.var/app/community.pathofbuilding.PathOfBuilding/data/RustyPathOfBuilding1"_s));
        QString why;
        QVERIFY(!findPob(computer.places, {}, &why));
        QCOMPARE(why, u"Open Path of Building Community (PoE2) once, so it downloads what it needs."_s);
    }

    void notInstalled()
    {
        Computer computer;
        const QString install = u"Path of Building isn't installed. Get it from Flathub with: "
                                u"flatpak install flathub community.pathofbuilding.PathOfBuilding"_s;
        QString why;
        QVERIFY(!findPob(computer.places, {}, &why));
        QCOMPARE(why, install);

        // nor with its folder but nothing deployed in it
        QVERIFY(QDir().mkpath(computer.places.flatpakInstallations.last() +
                              u"/app/community.pathofbuilding.PathOfBuilding"_s));
        QVERIFY(addPob(computer.flatpakFolder()));
        why.clear();
        QVERIFY(!findPob(computer.places, {}, &why));
        QCOMPARE(why, install);

        // nor without flatpak to run it
        QVERIFY(computer.installFlatpak());
        computer.places.flatpak.clear();
        why.clear();
        QVERIFY(!findPob(computer.places, {}, &why));
        QCOMPARE(why, install);
    }

    void thisComputer()
    {
        qputenv("FLATPAK_USER_DIR", "/srv/flatpak/user");
        qputenv("FLATPAK_SYSTEM_DIR", "/srv/flatpak/system");
        QCOMPARE(pobPlaces().flatpakInstallations,
                 (QStringList{u"/srv/flatpak/user"_s, u"/srv/flatpak/system"_s}));

        qunsetenv("FLATPAK_USER_DIR");
        qunsetenv("FLATPAK_SYSTEM_DIR");
        const PobPlaces places = pobPlaces();
        QCOMPARE(places.home, QDir::homePath());
        QCOMPARE(places.flatpakInstallations,
                 (QStringList{places.dataHome + u"/flatpak"_s, u"/var/lib/flatpak"_s}));
    }

    void withoutFlatpak()
    {
        Computer computer;
        const QString folder = computer.places.dataHome + u"/RustyPathOfBuilding2"_s;
        QVERIFY(addPob(folder));
        const std::optional<PobInstall> pob = findPob(computer.places, {});
        QVERIFY(pob);
        QCOMPARE(pob->folder, folder);
        QCOMPARE(pob->program, u"/usr/bin/luajit"_s);
        QVERIFY(pob->arguments.isEmpty());
        QVERIFY(!pob->flatpak);
        QCOMPARE(pob->userFolder, folder + u"/userdata/Path of Building (PoE2)"_s);

        computer.places.luajit.clear();
        QString why;
        QVERIFY(!findPob(computer.places, {}, &why));
        QCOMPARE(why, u"Running Path of Building needs LuaJIT, which isn't installed."_s);
    }

    void flatpakFirst()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        QVERIFY(addPob(computer.places.dataHome + u"/RustyPathOfBuilding2"_s));
        std::optional<PobInstall> pob = findPob(computer.places, {});
        QVERIFY(pob);
        QVERIFY(pob->flatpak);

        // unless it hasn't been opened for Path of Exile 2
        QVERIFY(QFile::remove(computer.flatpakFolder() + u"/HeadlessWrapper.lua"_s));
        pob = findPob(computer.places, {});
        QVERIFY(pob);
        QVERIFY(!pob->flatpak);
    }

    void location()
    {
        Computer computer;
        // the folder you pick, over the Flatpak
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        const QString folder = computer.dir.filePath(u"pob"_s);
        QVERIFY(addPob(folder));
        const std::optional<PobInstall> pob = findPob(computer.places, folder);
        QVERIFY(pob);
        QCOMPARE(pob->folder, folder);
        QCOMPARE(pob->program, u"/usr/bin/luajit"_s);
        QVERIFY(pob->arguments.isEmpty());
        QVERIFY(!pob->flatpak);
        QCOMPARE(pob->userFolder, folder + u"/userdata/Path of Building (PoE2)"_s);

        QCOMPARE(findPob(computer.places, folder + u'/'), pob);
    }

    void nothingInLocation()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        const QString folder = computer.dir.filePath(u"empty"_s);
        QVERIFY(QDir().mkpath(folder));
        QString why;
        QVERIFY(!findPob(computer.places, folder, &why));
        QCOMPARE(why, u"There's no Path of Building in %1."_s.arg(folder));
    }

    void locationWithoutLuajit()
    {
        Computer computer;
        const QString folder = computer.dir.filePath(u"pob"_s);
        QVERIFY(addPob(folder));
        computer.places.luajit.clear();
        QString why;
        QVERIFY(!findPob(computer.places, folder, &why));
        QCOMPARE(why,
                 u"Running the Path of Building in %1 needs LuaJIT, which isn't installed."_s.arg(folder));
    }

    void locationForPoe1()
    {
        Computer computer;
        const QString folder = computer.dir.filePath(u"RustyPathOfBuilding1"_s);
        QVERIFY(addPob(folder, poe1Manifest));
        QString why;
        QVERIFY(!findPob(computer.places, folder, &why));
        QCOMPARE(why, u"The Path of Building in %1 is for Path of Exile 1."_s.arg(folder));

        // which can't be told without its manifest
        QVERIFY(QFile::remove(folder + u"/manifest.xml"_s));
        QVERIFY(findPob(computer.places, folder));
    }

    void repositoryCopy()
    {
        Computer computer;
        const QString root = computer.dir.filePath(u"PathOfBuilding-PoE2"_s);
        QVERIFY(writeFile(root + u"/manifest.xml"_s, poe2Manifest));
        QVERIFY(addPob(root + u"/src"_s));
        // picked at the top or at src
        QCOMPARE(findPob(computer.places, root).value().folder, root + u"/src"_s);
        QCOMPARE(findPob(computer.places, root + u"/src"_s).value().folder, root + u"/src"_s);

        QVERIFY(writeFile(root + u"/manifest.xml"_s, poe1Manifest));
        QString why;
        QVERIFY(!findPob(computer.places, root, &why));
        QCOMPARE(why, u"The Path of Building in %1 is for Path of Exile 1."_s.arg(root));
    }

    void windows()
    {
        WindowsComputer computer;
        QVERIFY(addPob(computer.installFolder()));
        QVERIFY(writeFile(computer.installFolder() + u"/installed.cfg"_s, {}));
        QString why;
        const std::optional<PobInstall> pob = findPob(computer.places, {}, &why);
        QVERIFY2(pob, qPrintable(why));
        QCOMPARE(pob->folder, computer.installFolder());
        QCOMPARE(pob->program, u"C:/Program Files/Path of Upgrading/luajit.exe"_s);
        QVERIFY(pob->arguments.isEmpty());
        QVERIFY(!pob->flatpak);
        // as installed, it keeps what's yours in your Documents
        const QString user = computer.places.documents + u"/Path of Building (PoE2)"_s;
        QCOMPARE(pob->userFolder, user);
        QCOMPARE(pobBuildsFolder(*pob), user + u"/Builds"_s);
    }

    void windowsNotInstalled()
    {
        WindowsComputer computer;
        // not where rusty-path-of-building would have it
        QVERIFY(addPob(computer.places.dataHome + u"/RustyPathOfBuilding2"_s));
        QString why;
        QVERIFY(!findPob(computer.places, {}, &why));
        QCOMPARE(why, u"Path of Building isn't installed. Get Path of Building Community (PoE2) from: "
                      u"https://github.com/PathOfBuildingCommunity/PathOfBuilding-PoE2/releases"_s);
    }

    void windowsWithoutLuajit()
    {
        WindowsComputer computer;
        QVERIFY(addPob(computer.installFolder()));
        computer.places.luajit.clear();
        QString why;
        QVERIFY(!findPob(computer.places, {}, &why));
        QCOMPARE(why,
                 u"LuaJIT is missing from Path of Upgrading's folder. Installing it again puts it back."_s);
    }

    void windowsUnzipped()
    {
        WindowsComputer computer;
        // with no installed.cfg, it keeps what's yours in its own folder
        const QString folder = computer.dir.filePath(u"Games/PathOfBuilding-PoE2"_s);
        QVERIFY(addPob(folder));
        const std::optional<PobInstall> pob = findPob(computer.places, folder);
        QVERIFY(pob);
        QCOMPARE(pob->folder, folder);
        QCOMPARE(pob->userFolder, folder);
        QCOMPARE(pobBuildsFolder(*pob), folder + u"/Builds"_s);
    }

    void problems()
    {
        Computer computer;
        QCOMPARE(lookForPob(computer.places, {}).problem, PobProblem::NotInstalled);

        // the Flatpak is there, without Path of Building downloaded
        QVERIFY(computer.installFlatpak());
        QCOMPARE(lookForPob(computer.places, {}).problem, PobProblem::NotOpened);

        // Path of Building in the data folder, with nothing to run it
        Computer native;
        QVERIFY(addPob(native.places.dataHome + u"/RustyPathOfBuilding2"_s));
        native.places.luajit.clear();
        QCOMPARE(lookForPob(native.places, {}).problem, PobProblem::NoLuajit);

        // in the words findPob gives
        QString why;
        QVERIFY(!findPob(native.places, {}, &why));
        QCOMPARE(lookForPob(native.places, {}).why, why);
        QVERIFY(lookForPob(computer.places, {}).install == std::nullopt);
    }

    void folderProblems()
    {
        Computer computer;
        const QString empty = computer.dir.filePath(u"empty"_s);
        QVERIFY(QDir().mkpath(empty));
        QCOMPARE(lookForPob(computer.places, empty).problem, PobProblem::NothingInFolder);

        const QString poe1 = computer.dir.filePath(u"poe1"_s);
        QVERIFY(addPob(poe1, poe1Manifest));
        QCOMPARE(lookForPob(computer.places, poe1).problem, PobProblem::ForPoe1);

        const QString folder = computer.dir.filePath(u"pob"_s);
        QVERIFY(addPob(folder));
        computer.places.luajit.clear();
        QCOMPARE(lookForPob(computer.places, folder).problem, PobProblem::FolderNoLuajit);

        // the folder you set, even when the Flatpak is fine
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        QVERIFY(!lookForPob(computer.places, empty).install);
    }

    void windowsProblems()
    {
        WindowsComputer computer;
        QCOMPARE(lookForPob(computer.places, {}).problem, PobProblem::NotInstalledOnWindows);
        QVERIFY(addPob(computer.installFolder()));
        computer.places.luajit.clear();
        QCOMPARE(lookForPob(computer.places, {}).problem, PobProblem::NoLuajitOnWindows);
    }

    void buildsFolder()
    {
        Computer computer;
        const QString user = computer.flatpakFolder() + u"/userdata/Path of Building (PoE2)"_s;
        const PobInstall pob{.folder = computer.flatpakFolder(), .userFolder = user};
        QCOMPARE(pobBuildsFolder(pob), user + u"/Builds"_s);

        // with the rest of its settings, which aren't read
        const QString elsewhere = computer.dir.filePath(u"home/Builds"_s);
        const QByteArray settings = "<PathOfBuilding><Mode mode=\"LIST\"/><Accounts>"
                                    "<Account accountName=\"Someone\" sessionID=\"fake\"/></Accounts>"
                                    "<Misc buildSortMode=\"NAME\" buildPath=\"" +
                                    elsewhere.toUtf8() + "/\"/></PathOfBuilding>";
        QVERIFY(writeFile(user + u"/Settings.xml"_s, settings));
        // which isn't there yet
        QCOMPARE(pobBuildsFolder(pob), user + u"/Builds"_s);
        QVERIFY(QDir().mkpath(elsewhere));
        QCOMPARE(pobBuildsFolder(pob), elsewhere);
    }

    void buildsFolderNotChosen()
    {
        Computer computer;
        const QString user = computer.flatpakFolder() + u"/userdata/Path of Building (PoE2)"_s;
        const PobInstall pob{.folder = computer.flatpakFolder(), .userFolder = user};
        // only its own options count
        const QByteArray settings = "<PathOfBuilding><SharedItems><Misc buildPath=\"" +
                                    computer.dir.path().toUtf8() +
                                    "\"/></SharedItems><Misc buildSortMode=\"NAME\"/></PathOfBuilding>";
        QVERIFY(writeFile(user + u"/Settings.xml"_s, settings));
        QCOMPARE(pobBuildsFolder(pob), user + u"/Builds"_s);
    }

    void listsBuilds()
    {
        QTemporaryDir dir;
        const QString folder = dir.filePath(u"Builds"_s);
        const QDateTime now = QDateTime::currentDateTime();
        const auto save = [&](const QString &name, int daysAgo) {
            const QString path = folder + u'/' + name;
            QFile file(path);
            return writeFile(path, "<PathOfBuilding2/>") && file.open(QIODevice::ReadWrite) &&
                   file.setFileTime(now.addDays(-daysAgo), QFileDevice::FileModificationTime);
        };
        QVERIFY(save(u"Witch.xml"_s, 3));
        QVERIFY(save(u"Leveling/Monk.xml"_s, 1));
        QVERIFY(save(u"Leveling/Old/Ranger.xml"_s, 9));
        QVERIFY(save(u"Mercenary.xml"_s, 3));
        // saved the same day, so by name
        QVERIFY(save(u"Zealot.xml"_s, 3));
        QVERIFY(save(u"Acolyte.xml"_s, 3));
        QVERIFY(writeFile(folder + u"/notes.txt"_s, "not a build"));

        const QList<SavedBuild> builds = savedBuilds(folder);
        QStringList names;
        for (const SavedBuild &build : builds)
            names << build.name;
        QCOMPARE(names, (QStringList{u"Leveling/Monk"_s, u"Acolyte"_s, u"Mercenary"_s, u"Witch"_s,
                                     u"Zealot"_s, u"Leveling/Old/Ranger"_s}));
        QCOMPARE(builds.first().path, folder + u"/Leveling/Monk.xml"_s);
        QCOMPARE(builds.first().modified.toSecsSinceEpoch(), now.addDays(-1).toSecsSinceEpoch());

        QVERIFY(savedBuilds(dir.filePath(u"nothing here"_s)).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPobInstall)
#include "tst_pobinstall.moc"
