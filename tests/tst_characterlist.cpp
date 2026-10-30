// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/characterlist.h"
#include "app/settingsstore.h"
#include "itemtexts.h"
#include "services/pobengine.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString stormweaver = u"<PathOfBuilding2><Build level=\"92\" className=\"Sorceress\" "
                            u"ascendClassName=\"Stormweaver\"/></PathOfBuilding2>"_s;

Character named(const QString &name)
{
    Character c;
    c.name = name;
    return c;
}

// Your characters, as the window lists them.
struct Characters
{
    QTemporaryDir dir;
    QString folder = dir.filePath(u"characters"_s);
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    CharacterStore store{folder};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    CharacterList list{&store, &active, &settings};
    QAbstractItemModelTester tester{&list, QAbstractItemModelTester::FailureReportingMode::QtTest};

    // one saved before the list reads them
    QString saved(const QString &name, const QString &xml = {})
    {
        CharacterStore before(folder);
        before.load();
        const QString id = before.add(named(name)).value_or(QString());
        if (!xml.isEmpty())
            before.savePobBuild(id, xml);
        return id;
    }

    QStringList names() const
    {
        QStringList names;
        for (int row = 0; row < list.rowCount(); ++row)
            names << list.index(row).data(CharacterList::NameRole).toString();
        return names;
    }

    void play(const QString &id)
    {
        Settings s = settings.settings();
        s.character = id;
        settings.set(s);
    }

    // as it is in the folder
    const Character *inFolder(const QString &id)
    {
        again = std::make_unique<CharacterStore>(folder);
        again->load();
        return again->find(id);
    }
    std::unique_ptr<CharacterStore> again;
};

} // namespace

class TestCharacterList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void buildName_data()
    {
        QTest::addColumn<int>("level");
        QTest::addColumn<QString>("className");
        QTest::addColumn<QString>("ascendancy");
        QTest::addColumn<QString>("name");
        QTest::newRow("ascended") << 92 << u"Sorceress"_s << u"Stormweaver"_s << u"level 92 Stormweaver"_s;
        QTest::newRow("not yet") << 12 << u"Sorceress"_s << QString() << u"level 12 Sorceress"_s;
        QTest::newRow("no level") << 0 << u"Sorceress"_s << u"Stormweaver"_s << u"Stormweaver"_s;
        QTest::newRow("no class") << 3 << QString() << QString() << u"level 3"_s;
        QTest::newRow("nothing") << 0 << QString() << QString() << QString();
    }

    void buildName()
    {
        QFETCH(int, level);
        QFETCH(QString, className);
        QFETCH(QString, ascendancy);
        QFETCH(QString, name);
        PobBuild build;
        build.level = level;
        build.className = className;
        build.ascendancy = ascendancy;
        QCOMPARE(pou::buildName(build), name);
    }

    void listsThem()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s, stormweaver);
        c.saved(u"Torvald"_s);
        c.saved(u"anya"_s, u"<PathOfBuilding><Build level=\"90\"/></PathOfBuilding>"_s);
        QCOMPARE(c.list.rowCount(), 0);

        QSignalSpy loaded(&c.list, &CharacterList::loaded);
        c.list.load();
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(c.names(), (QStringList{u"anya"_s, u"Torvald"_s, u"Wren"_s}));
        const QModelIndex row = c.list.index(2);
        QCOMPARE(c.list.rowCount(row), 0);
        QCOMPARE(row.data(CharacterList::IdRole).toString(), wren);
        QCOMPARE(row.data(Qt::DisplayRole).toString(), u"Wren"_s);
        QCOMPARE(row.data(CharacterList::BuildRole).toString(), u"Level 92 Stormweaver"_s);
        // a build for the first game says nothing
        QCOMPARE(c.list.index(0).data(CharacterList::BuildRole).toString(), QString());
        QCOMPARE(c.list.index(1).data(CharacterList::BuildRole).toString(), QString());
        QVERIFY(c.list.problems().isEmpty());

        const QHash<int, QByteArray> roles = c.list.roleNames();
        QCOMPARE(roles.value(CharacterList::IdRole), "characterId");
        QCOMPARE(roles.value(CharacterList::NameRole), "name");
        QCOMPARE(roles.value(CharacterList::BuildRole), "build");
    }

    void buildWithNothingToName()
    {
        Characters c;
        c.saved(u"Wren"_s, u"<PathOfBuilding2><Build/></PathOfBuilding2>"_s);
        c.list.load();
        QCOMPARE(c.list.index(0).data(CharacterList::BuildRole).toString(), QString());
    }

    void problems()
    {
        Characters c;
        c.saved(u"Wren"_s);
        QFile broken(c.folder + u"/broken.json"_s);
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("{ not json");
        broken.close();
        c.list.load();
        QCOMPARE(c.names(), QStringList{u"Wren"_s});
        QCOMPARE(c.list.problems().size(), 1);
        QVERIFY(c.list.problems().first().startsWith(u"broken.json: "_s));
    }

    void loadsAgain()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s, stormweaver);
        c.list.load();
        QVERIFY(QFile::remove(c.folder + u'/' + wren + u".xml"_s));
        c.saved(u"Anya"_s);
        QSignalSpy reset(&c.list, &CharacterList::modelReset);
        c.list.load();
        QCOMPARE(reset.size(), 1);
        QCOMPARE(c.names(), (QStringList{u"Anya"_s, u"Wren"_s}));
        QCOMPARE(c.list.index(1).data(CharacterList::BuildRole).toString(), QString());
    }

    void current()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s);
        const QString torvald = c.saved(u"Torvald"_s);
        c.play(wren);
        CharacterList list(&c.store, &c.active, &c.settings);
        list.load();
        QCOMPARE(list.current(), wren);

        QSignalSpy changed(&list, &CharacterList::currentChanged);
        list.setCurrent(torvald);
        QCOMPARE(c.settings.settings().character, torvald);
        QCOMPARE(list.current(), torvald);
        QCOMPARE(changed.size(), 1);

        // and when the settings change some other way
        c.play(wren);
        QCOMPARE(list.current(), wren);
        QCOMPARE(changed.size(), 2);
        c.play(wren);
        QCOMPARE(changed.size(), 2);
        Settings settings = c.settings.settings();
        settings.trade.enabled = false;
        c.settings.set(settings);
        QCOMPARE(changed.size(), 2);

        list.setCurrent(u"nobody"_s);
        QCOMPARE(list.current(), wren);
        list.setCurrent({});
        QCOMPARE(c.settings.settings().character, QString());
        QCOMPARE(list.current(), QString());
    }

    void add()
    {
        Characters c;
        c.saved(u"Anya"_s);
        c.saved(u"Wren"_s);
        c.list.load();
        QSignalSpy inserted(&c.list, &CharacterList::rowsInserted);
        QSignalSpy current(&c.list, &CharacterList::currentChanged);

        const QString id = c.list.add(u"  Mira "_s);
        QVERIFY(!id.isEmpty());
        QCOMPARE(c.names(), (QStringList{u"Anya"_s, u"Mira"_s, u"Wren"_s}));
        QCOMPARE(inserted.size(), 1);
        QCOMPARE(inserted.first().at(1).toInt(), 1);
        QCOMPARE(c.list.current(), id);
        QCOMPARE(current.size(), 1);
        QVERIFY(c.inFolder(id));
        QCOMPARE(c.inFolder(id)->name, u"Mira"_s);
        QVERIFY(c.inFolder(id)->gear.isEmpty());
        QVERIFY(c.list.error().isEmpty());
    }

    void addWithoutName()
    {
        Characters c;
        c.list.load();
        QSignalSpy error(&c.list, &CharacterList::errorChanged);
        QCOMPARE(c.list.add(u"  "_s), QString());
        QCOMPARE(c.list.error(), u"Give the character a name."_s);
        QCOMPARE(error.size(), 1);
        QCOMPARE(c.list.rowCount(), 0);
        QCOMPARE(c.list.current(), QString());

        c.list.add({});
        QCOMPARE(error.size(), 1);

        // and it's forgotten once one is added
        QVERIFY(!c.list.add(u"Mira"_s).isEmpty());
        QCOMPARE(c.list.error(), QString());
        QCOMPARE(error.size(), 2);
        QVERIFY(!c.list.add(u"Wren"_s).isEmpty());
        QCOMPARE(error.size(), 2);
    }

    void addFails()
    {
        Characters c;
        // a file where the folder would be
        QFile file(c.folder);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        c.list.load();
        QCOMPARE(c.list.add(u"Mira"_s), QString());
        QCOMPARE(c.list.error(), u"Couldn't save Mira: can't make the folder %1"_s.arg(c.folder));
        QCOMPARE(c.list.rowCount(), 0);
        QCOMPARE(c.list.current(), QString());
    }

    void rename()
    {
        Characters c;
        const QString anya = c.saved(u"Anya"_s);
        c.saved(u"Mira"_s);
        c.saved(u"Wren"_s);
        c.list.load();
        QSignalSpy moved(&c.list, &CharacterList::rowsMoved);
        QSignalSpy changed(&c.list, &CharacterList::dataChanged);

        QVERIFY(c.list.rename(anya, u" Zora "_s));
        QCOMPARE(c.names(), (QStringList{u"Mira"_s, u"Wren"_s, u"Zora"_s}));
        QCOMPARE(moved.size(), 1);
        // to before the row after the last one
        QCOMPARE(moved.first().at(1).toInt(), 0);
        QCOMPARE(moved.first().at(4).toInt(), 3);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().first().toModelIndex().row(), 2);
        QCOMPARE(c.inFolder(anya)->name, u"Zora"_s);

        // and back up
        QVERIFY(c.list.rename(anya, u"Abby"_s));
        QCOMPARE(c.names(), (QStringList{u"Abby"_s, u"Mira"_s, u"Wren"_s}));
        QCOMPARE(moved.size(), 2);
        QCOMPARE(moved.last().at(1).toInt(), 2);
        QCOMPARE(moved.last().at(4).toInt(), 0);

        // staying where it is
        QVERIFY(c.list.rename(anya, u"Aby"_s));
        QCOMPARE(c.names(), (QStringList{u"Aby"_s, u"Mira"_s, u"Wren"_s}));
        QCOMPARE(moved.size(), 2);
        QCOMPARE(changed.size(), 3);
        QCOMPARE(changed.last().first().toModelIndex().row(), 0);
    }

    void renameFails()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s);
        c.list.load();
        // a folder with something in it where the character's file would be
        const QString file = c.folder + u'/' + wren + u".json"_s;
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir(c.folder).mkpath(wren + u".json/kept"_s));
        QVERIFY(!c.list.rename(wren, u"Wrenna"_s));
        QVERIFY(c.list.error().startsWith(u"Couldn't save Wrenna: "_s));
        QCOMPARE(c.names(), QStringList{u"Wren"_s});

        // and nothing is wrong once it works
        QVERIFY(QDir(file).removeRecursively());
        QVERIFY(c.list.rename(wren, u"Wrenna"_s));
        QCOMPARE(c.list.error(), QString());
        QCOMPARE(c.names(), QStringList{u"Wrenna"_s});
    }

    void renameTheOneYouPlay()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s, stormweaver);
        c.list.load();
        c.active.setId(wren);
        QSignalSpy reloaded(&c.active, &ActiveCharacter::changed);
        QVERIFY(c.list.rename(wren, u"Wrenna"_s));
        QCOMPARE(reloaded.size(), 1);
        QCOMPARE(c.active.character().name, u"Wrenna"_s);

        // not for one you don't play
        const QString mira = c.list.add(u"Mira"_s);
        c.active.setId(wren);
        reloaded.clear();
        QVERIFY(c.list.rename(mira, u"Mirabel"_s));
        QCOMPARE(reloaded.size(), 0);
    }

    void renameNot()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s);
        c.list.load();
        QSignalSpy changed(&c.list, &CharacterList::dataChanged);
        QVERIFY(c.list.rename(wren, u"Wren"_s));
        QCOMPARE(changed.size(), 0);
        QVERIFY(!c.list.rename(wren, u" "_s));
        QCOMPARE(c.list.error(), u"Give the character a name."_s);
        QCOMPARE(c.inFolder(wren)->name, u"Wren"_s);

        // the name it has is fine, so the error goes
        QVERIFY(c.list.rename(wren, u"Wren"_s));
        QCOMPARE(c.list.error(), QString());
        QVERIFY(!c.list.rename(u"nobody"_s, u"Wren"_s));
        QCOMPARE(changed.size(), 0);
    }

    void remove()
    {
        Characters c;
        const QString anya = c.saved(u"Anya"_s);
        const QString mira = c.saved(u"Mira"_s, stormweaver);
        const QString wren = c.saved(u"Wren"_s);
        c.play(wren);
        c.list.load();
        QSignalSpy removed(&c.list, &CharacterList::rowsRemoved);

        QVERIFY(c.list.remove(mira));
        QCOMPARE(c.names(), (QStringList{u"Anya"_s, u"Wren"_s}));
        QCOMPARE(removed.size(), 1);
        QCOMPARE(removed.first().at(1).toInt(), 1);
        QVERIFY(!c.inFolder(mira));
        QVERIFY(!QFile::exists(c.folder + u'/' + mira + u".xml"_s));
        QCOMPARE(c.list.current(), wren);

        // you then play the first of the others
        QVERIFY(c.list.remove(wren));
        QCOMPARE(c.list.current(), anya);
        QCOMPARE(c.settings.settings().character, anya);
        QVERIFY(c.list.remove(anya));
        QCOMPARE(c.list.current(), QString());
        QCOMPARE(c.list.rowCount(), 0);

        QVERIFY(!c.list.remove(anya));
    }

    void removeFails()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s);
        c.list.load();
        // a folder with something in it where the build would be, which
        // can't be deleted like a file
        QVERIFY(QDir(c.folder).mkpath(wren + u".xml/kept"_s));
        QVERIFY(!c.list.remove(wren));
        QVERIFY(c.list.error().startsWith(u"Couldn't delete Wren: "_s));
        QCOMPARE(c.names(), QStringList{u"Wren"_s});

        // and nothing is wrong once it works
        QVERIFY(QDir(c.folder + u'/' + wren + u".xml"_s).removeRecursively());
        QVERIFY(c.list.remove(wren));
        QCOMPARE(c.list.error(), QString());
    }

    void importBuild()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s);
        c.list.load();
        c.active.setId(wren);
        QSignalSpy changed(&c.list, &CharacterList::dataChanged);
        QSignalSpy reloaded(&c.active, &ActiveCharacter::changed);

        PobBuild build = parsePobBuild(stormweaver).value();
        build.gear.insert(u"Helmet"_s, {.text = itemtexts::helmetNew});
        const std::optional<BuildImport> done = c.list.importBuild(wren, stormweaver, build);
        QVERIFY(done);
        QCOMPARE(done->items, 1);
        QCOMPARE(done->character.gear.value(u"Helmet"_s).name, u"Doom Visage"_s);

        QCOMPARE(changed.size(), 1);
        QCOMPARE(c.list.index(0).data(CharacterList::BuildRole).toString(), u"Level 92 Stormweaver"_s);
        QCOMPARE(c.inFolder(wren)->gear.value(u"Helmet"_s).name, u"Doom Visage"_s);
        QCOMPARE(c.inFolder(wren)->fromPob, QSet<QString>{u"Helmet"_s});
        QCOMPARE(c.store.pobBuild(wren).value_or(QString()), stormweaver);
        QCOMPARE(reloaded.size(), 1);
        QCOMPARE(c.active.build()->level, 92);

        QVERIFY(!c.list.importBuild(u"nobody"_s, stormweaver, build));
    }

    void importFitsTheProfile()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s);
        c.list.load();
        c.active.setId(wren);
        PobBuild build = parsePobBuild(stormweaver).value();
        build.life = 900;
        build.energyShield = 4500;

        const std::optional<BuildImport> done = c.list.importBuild(wren, stormweaver, build);
        QVERIFY(done);
        QVERIFY(done->profileChanged);
        QCOMPARE(c.inFolder(wren)->buildProfile, u"es-caster"_s);
        QVERIFY(!c.inFolder(wren)->profilePicked);
        QCOMPARE(c.active.character().buildProfile, u"es-caster"_s);

        // and the next build's, as long as you haven't chosen
        build.minionDps = 800;
        QVERIFY(c.list.importBuild(wren, stormweaver, build));
        QCOMPARE(c.inFolder(wren)->buildProfile, u"minion-es"_s);
    }

    void importLeavesYourProfile()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s);
        c.list.load();
        Character picked = *c.store.find(wren);
        picked.buildProfile = u"life-res"_s;
        picked.profilePicked = true;
        QVERIFY(c.store.save(wren, picked));
        c.active.setId(wren);
        PobBuild build = parsePobBuild(stormweaver).value();
        build.energyShield = 4500;

        const std::optional<BuildImport> done = c.list.importBuild(wren, stormweaver, build);
        QVERIFY(done);
        QVERIFY(!done->profileChanged);
        QCOMPARE(c.inFolder(wren)->buildProfile, u"life-res"_s);
        QVERIFY(c.inFolder(wren)->profilePicked);
    }

    void importFails()
    {
        Characters c;
        const QString wren = c.saved(u"Wren"_s);
        c.list.load();
        // a folder where the build would be
        QVERIFY(QDir(c.folder).mkdir(wren + u".xml"_s));
        const std::optional<BuildImport> done =
            c.list.importBuild(wren, stormweaver, parsePobBuild(stormweaver).value());
        QVERIFY(!done);
        QVERIFY(c.list.error().startsWith(u"Couldn't save the build for Wren: "_s));
        QCOMPARE(c.list.index(0).data(CharacterList::BuildRole).toString(), QString());

        // and nothing is wrong once it works
        QVERIFY(QDir(c.folder).rmdir(wren + u".xml"_s));
        QVERIFY(c.list.importBuild(wren, stormweaver, parsePobBuild(stormweaver).value()));
        QCOMPARE(c.list.error(), QString());
    }
};

QTEST_GUILESS_MAIN(TestCharacterList)
#include "tst_characterlist.moc"
