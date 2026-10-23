// SPDX-License-Identifier: MIT
#include "core/datafiles.h"
#include "core/pobinstall.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

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

// Path of Building's files, as far as finding it goes.
bool addPob(const QString &folder)
{
    return writeFile(folder + u"/HeadlessWrapper.lua"_s, {});
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
        const std::optional<PobInstall> pob = findPob(computer.places, &why);
        QVERIFY2(pob, qPrintable(why));
        QCOMPARE(pob->folder, computer.flatpakFolder());
        QCOMPARE(pob->program, u"/usr/bin/flatpak"_s);
        QCOMPARE(pob->arguments, flatpakArguments);
        QVERIFY(pob->flatpak);
    }

    void flatpakForYou()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak(true));
        QVERIFY(addPob(computer.flatpakFolder()));
        QVERIFY(findPob(computer.places));
    }

    void flatpakNotOpened()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        // only for Path of Exile 1
        QVERIFY(addPob(computer.places.home +
                       u"/.var/app/community.pathofbuilding.PathOfBuilding/data/RustyPathOfBuilding1"_s));
        QString why;
        QVERIFY(!findPob(computer.places, &why));
        QCOMPARE(why, u"Open Path of Building PoE 2 once, so it downloads what it needs."_s);
    }

    void notInstalled()
    {
        Computer computer;
        const QString install = u"Path of Building isn't installed. Get it from Flathub with: "
                                u"flatpak install flathub community.pathofbuilding.PathOfBuilding"_s;
        QString why;
        QVERIFY(!findPob(computer.places, &why));
        QCOMPARE(why, install);

        // nor with its folder but nothing deployed in it
        QVERIFY(QDir().mkpath(computer.places.flatpakInstallations.last() +
                              u"/app/community.pathofbuilding.PathOfBuilding"_s));
        QVERIFY(addPob(computer.flatpakFolder()));
        why.clear();
        QVERIFY(!findPob(computer.places, &why));
        QCOMPARE(why, install);

        // nor without flatpak to run it
        QVERIFY(computer.installFlatpak());
        computer.places.flatpak.clear();
        why.clear();
        QVERIFY(!findPob(computer.places, &why));
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
        const std::optional<PobInstall> pob = findPob(computer.places);
        QVERIFY(pob);
        QCOMPARE(pob->folder, folder);
        QCOMPARE(pob->program, u"/usr/bin/luajit"_s);
        QVERIFY(pob->arguments.isEmpty());
        QVERIFY(!pob->flatpak);

        computer.places.luajit.clear();
        QString why;
        QVERIFY(!findPob(computer.places, &why));
        QCOMPARE(why, u"Running Path of Building needs LuaJIT, which isn't installed."_s);
    }

    void flatpakFirst()
    {
        Computer computer;
        QVERIFY(computer.installFlatpak());
        QVERIFY(addPob(computer.flatpakFolder()));
        QVERIFY(addPob(computer.places.dataHome + u"/RustyPathOfBuilding2"_s));
        std::optional<PobInstall> pob = findPob(computer.places);
        QVERIFY(pob);
        QVERIFY(pob->flatpak);

        // unless it hasn't been opened for Path of Exile 2
        QVERIFY(QFile::remove(computer.flatpakFolder() + u"/HeadlessWrapper.lua"_s));
        pob = findPob(computer.places);
        QVERIFY(pob);
        QVERIFY(!pob->flatpak);
    }
};

QTEST_GUILESS_MAIN(TestPobInstall)
#include "tst_pobinstall.moc"
