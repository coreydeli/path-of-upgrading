// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/buildimporter.h"
#include "app/characterlist.h"
#include "app/settingsstore.h"
#include "core/datafiles.h"
#include "core/parser.h"
#include "fakenetwork.h"
#include "pobfixtures.h"
#include "services/pobengine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// as Path of Building shares them, without qCompress's length in front
QString codeFor(const QString &xml)
{
    return QString::fromLatin1(qCompress(xml.toUtf8()).mid(4).toBase64(QByteArray::Base64UrlEncoding));
}

const QString bow = u"Rarity: RARE\nGrim Arc\nCrude Bow\nImplicits: 0\n+10 to Dexterity"_s;

// A level 40 Ranger with the bow in each of the slots, Path of Building's:
// "Weapon 2" is our "Offhand".
QString ranger(const QStringList &slots)
{
    QString items;
    QString set;
    for (qsizetype i = 0; i < slots.size(); ++i) {
        items += u"<Item id=\"%1\">%2</Item>"_s.arg(QString::number(i + 1), bow);
        set += u"<Slot name=\"%1\" itemId=\"%2\"/>"_s.arg(slots.at(i), QString::number(i + 1));
    }
    return u"<PathOfBuilding2><Build level=\"40\" className=\"Ranger\"/><Items activeItemSet=\"1\">%1"
           u"<ItemSet id=\"1\">%2</ItemSet></Items></PathOfBuilding2>"_s.arg(items, set);
}

// A level 70 Witch with five times as much Energy Shield as Life, and
// minions that deal damage when asked to.
QString witch(bool withMinions = false)
{
    return u"<PathOfBuilding2><Build level=\"70\" className=\"Witch\">"
           u"<PlayerStat stat=\"Life\" value=\"900\"/><PlayerStat stat=\"EnergyShield\" value=\"4500\"/>"_s +
           (withMinions ? u"<MinionStat stat=\"TotalDPS\" value=\"12000\"/>"_s : QString()) +
           u"</Build></PathOfBuilding2>"_s;
}

// the bow as the game copies it
Item copiedBow()
{
    return parseItem(u"Item Class: Bows\nRarity: Rare\nGrim Arc\nCrude Bow\n--------\n"
                     u"Physical Damage: 7-13\n--------\n+10 to Dexterity"_s)
        .value();
}

struct Importer
{
    QTemporaryDir dir;
    QString folder = dir.filePath(u"characters"_s);
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    CharacterStore store{folder};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    CharacterList list{&store, &active, &settings};
    FakeNetwork network;
    BuildImporter importer{&list, &engine, &network};

    Importer() { list.load(); }

    // the one you play
    QString play(const QString &name, const Equipped &gear = {})
    {
        const QString id = list.add(name);
        if (!gear.isEmpty()) {
            Character c = *store.find(id);
            c.gear = gear;
            store.save(id, c);
        }
        return id;
    }

    QString builds() const { return dir.filePath(u"pob/userdata/Path of Building (PoE2)/Builds"_s); }
};

} // namespace

class TestBuildImporter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLoggingCategory::setFilterRules(u"pathofupgrading.pob.info=false"_s); }
    void init() { QTest::failOnWarning(); }

    void importsACode()
    {
        Importer i;
        const QString wren = i.play(u"Wren"_s);
        QSignalSpy message(&i.importer, &BuildImporter::messageChanged);
        i.importer.importText(u"  "_s + codeFor(pobfixtures::build) + u"\n"_s);
        QCOMPARE(message.size(), 1);
        QVERIFY(!i.importer.hasFailed());
        QCOMPARE(i.importer.message(),
                 u"Imported the level 92 Stormweaver build for Wren. Wren now wears 8 items from it."_s);
        QCOMPARE(i.store.pobBuild(wren).value_or(QString()), pobfixtures::build);
        QCOMPARE(i.store.find(wren)->gear.size(), 8);
        QCOMPARE(i.list.rowCount(), 1);
        QVERIFY(!i.importer.isBusy());
    }

    void importsTheXml()
    {
        Importer i;
        const QString wren = i.play(u"Wren"_s);
        i.importer.importText(ranger({u"Weapon 1"_s}));
        QVERIFY(!i.importer.hasFailed());
        QCOMPARE(i.store.pobBuild(wren).value_or(QString()), ranger({u"Weapon 1"_s}));
    }

    void saysWhichProfile()
    {
        Importer i;
        const QString wren = i.play(u"Wren"_s);
        i.importer.importText(witch());
        QVERIFY(!i.importer.hasFailed());
        QCOMPARE(i.importer.message(), u"Imported the level 70 Witch build for Wren. It has no items on. "
                                       u"Chose the Energy Shield spellcaster gear score profile for it."_s);
        QCOMPARE(i.store.find(wren)->buildProfile, u"es-caster"_s);

        // nothing more to say when it's the one it has
        i.importer.importText(witch());
        QCOMPARE(i.importer.message(), u"Imported the level 70 Witch build for Wren. It has no items on."_s);

        i.importer.importText(witch(true));
        QVERIFY(i.importer.message().endsWith(
            u"Chose the Minion summoner on Energy Shield gear score profile for it."_s));
        QCOMPARE(i.store.find(wren)->buildProfile, u"minion-es"_s);
    }

    void saysNothingOfAPickedProfile()
    {
        Importer i;
        const QString wren = i.play(u"Wren"_s);
        Character picked = *i.store.find(wren);
        picked.buildProfile = u"life-res"_s;
        picked.profilePicked = true;
        QVERIFY(i.store.save(wren, picked));

        i.importer.importText(witch());
        QCOMPARE(i.importer.message(), u"Imported the level 70 Witch build for Wren. It has no items on."_s);
        QCOMPARE(i.store.find(wren)->buildProfile, u"life-res"_s);
    }

    void newCharacter_data()
    {
        QTest::addColumn<QString>("xml");
        QTest::addColumn<QString>("name");
        QTest::newRow("ascended") << pobfixtures::build << u"Stormweaver"_s;
        QTest::newRow("not yet") << ranger({}) << u"Ranger"_s;
        QTest::newRow("unknown") << u"<PathOfBuilding2/>"_s << u"New character"_s;
    }

    void newCharacter()
    {
        QFETCH(QString, xml);
        QFETCH(QString, name);
        Importer i;
        // and the one in the settings is gone
        i.list.setCurrent(i.play(u"Wren"_s));
        QVERIFY(i.list.remove(i.list.current()));
        Settings settings = i.settings.settings();
        settings.character = u"gone"_s;
        i.settings.set(settings);

        i.importer.importText(xml);
        QVERIFY2(!i.importer.hasFailed(), qUtf8Printable(i.importer.message()));
        QCOMPARE(i.list.rowCount(), 1);
        const QString id = i.list.current();
        QCOMPARE(i.store.find(id)->name, name);
        QVERIFY(i.store.pobBuild(id));
    }

    void rejects_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("why");
        QTest::newRow("nothing") << u" \n"_s << u"Paste a build code or a link to one first."_s;
        QTest::newRow("not a code") << u"not a code!"_s
                                    << u"That isn't a Path of Building code or a link to one."_s;
        QTest::newRow("another site")
            << u"https://example.com/AbC123"_s << u"That isn't a Path of Building code or a link to one."_s;
        QTest::newRow("first game") << codeFor(u"<PathOfBuilding><Build level=\"90\"/></PathOfBuilding>"_s)
                                    << u"That's a build for Path of Exile 1."_s;
        QTest::newRow("some xml") << u"<html><body/></html>"_s << u"That isn't a Path of Exile 2 build."_s;
        QTest::newRow("broken") << u"<PathOfBuilding2><Build>"_s << u"That isn't a Path of Exile 2 build."_s;
    }

    void rejects()
    {
        QFETCH(QString, text);
        QFETCH(QString, why);
        Importer i;
        QSignalSpy message(&i.importer, &BuildImporter::messageChanged);
        i.importer.importText(text);
        QCOMPARE(message.size(), 1);
        QVERIFY(i.importer.hasFailed());
        QCOMPARE(i.importer.message(), why);
        QCOMPARE(i.list.rowCount(), 0);
        QVERIFY(i.network.sent.isEmpty());
    }

    void says_data()
    {
        QTest::addColumn<QString>("xml");
        QTest::addColumn<bool>("wearingBows");
        QTest::addColumn<QString>("message");
        QTest::newRow("one")
            << ranger({u"Weapon 1"_s}) << false
            << u"Imported the level 40 Ranger build for Wren. Wren now wears 1 item from it. "
               u"Copy your weapon in the game once, as the build doesn't say its damage."_s;
        QTest::newRow("two")
            << ranger({u"Weapon 1"_s, u"Weapon 2"_s}) << false
            << u"Imported the level 40 Ranger build for Wren. Wren now wears 2 items from it. "
               u"Copy your weapons in the game once, as the build doesn't say their "
               u"damage."_s;
        QTest::newRow("kept one") << ranger({u"Weapon 1"_s}) << true
                                  << u"Imported the level 40 Ranger build for Wren. Kept 1 item you copied "
                                     u"in the game, which says more."_s;
        QTest::newRow("kept two") << ranger({u"Weapon 1"_s, u"Weapon 2"_s}) << true
                                  << u"Imported the level 40 Ranger build for Wren. Kept 2 items you copied "
                                     u"in the game, which say more."_s;
        QTest::newRow("kept one of two") << ranger({u"Weapon 1"_s, u"Ring 1"_s}) << true
                                         << u"Imported the level 40 Ranger build for Wren. Wren now wears 1 "
                                            u"item from it. Kept 1 item you copied in the game, which says "
                                            u"more."_s;
        QTest::newRow("empty") << u"<PathOfBuilding2/>"_s << false
                               << u"Imported the build for Wren. It has no items on."_s;
    }

    void says()
    {
        QFETCH(QString, xml);
        QFETCH(bool, wearingBows);
        QFETCH(QString, message);
        Importer i;
        Equipped gear;
        if (wearingBows)
            gear = {{u"Weapon"_s, copiedBow()}, {u"Offhand"_s, copiedBow()}};
        i.play(u"Wren"_s, gear);
        i.importer.importText(xml);
        QCOMPARE(i.importer.message(), message);
    }

    void fromALink()
    {
        Importer i;
        const QString wren = i.play(u"Wren"_s);
        i.importer.importText(u"no good"_s);
        QVERIFY(i.importer.hasFailed());

        i.network.answers = {{.body = codeFor(ranger({})).toLatin1()}};
        QSignalSpy busy(&i.importer, &BuildImporter::busyChanged);
        QSignalSpy message(&i.importer, &BuildImporter::messageChanged);
        i.importer.importText(u"https://pobb.in/AbC123"_s);
        QVERIFY(i.importer.isBusy());
        QCOMPARE(busy.size(), 1);
        // what was said before goes
        QCOMPARE(message.size(), 1);
        QCOMPARE(i.importer.message(), QString());
        QVERIFY(!i.importer.hasFailed());

        QVERIFY(message.wait());
        QVERIFY(!i.importer.isBusy());
        QCOMPARE(busy.size(), 2);
        QCOMPARE(i.importer.message(), u"Imported the level 40 Ranger build for Wren. It has no items on."_s);
        QCOMPARE(i.network.sent.first().request.url(), QUrl(u"https://pobb.in/pob/AbC123"_s));
        QCOMPARE(i.store.pobBuild(wren).value_or(QString()), ranger({}));
    }

    void newCharacterFromALink()
    {
        Importer i;
        i.network.answers = {{.body = codeFor(ranger({})).toLatin1()}};
        QSignalSpy message(&i.importer, &BuildImporter::messageChanged);
        i.importer.importText(u"https://pobb.in/AbC123"_s);
        QVERIFY(message.wait());
        QCOMPARE(i.store.find(i.list.current())->name, u"Ranger"_s);
    }

    void linkFails_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<QString>("why");
        QTest::newRow("no build") << FakeAnswer{.status = 404} << u"There's no build at that link."_s;
        QTest::newRow("not a code") << FakeAnswer{.body = "Sign in to see this paste."}
                                    << u"That link doesn't give a Path of Building code."_s;
    }

    void linkFails()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(QString, why);
        Importer i;
        i.play(u"Wren"_s);
        i.network.answers = {answer};
        QSignalSpy busy(&i.importer, &BuildImporter::busyChanged);
        QSignalSpy message(&i.importer, &BuildImporter::messageChanged);
        i.importer.importText(u"https://pobb.in/AbC123"_s);
        QVERIFY(message.wait());
        QCOMPARE(busy.size(), 2);
        QVERIFY(!i.importer.isBusy());
        QVERIFY(i.importer.hasFailed());
        QCOMPARE(i.importer.message(), why);
    }

    void inPlaceOfTheDownload()
    {
        Importer i;
        const QString wren = i.play(u"Wren"_s);
        i.network.answers = {{.body = codeFor(pobfixtures::build).toLatin1()}};
        i.importer.importText(u"https://pobb.in/AbC123"_s);
        QSignalSpy busy(&i.importer, &BuildImporter::busyChanged);
        QSignalSpy message(&i.importer, &BuildImporter::messageChanged);
        i.importer.importText(ranger({}));
        QVERIFY(!i.importer.isBusy());
        QCOMPARE(busy.size(), 1);
        QVERIFY(i.network.sent.first().reply.isNull());
        QCOMPARE(message.size(), 1);
        QTest::qWait(10);
        QCOMPARE(message.size(), 1);
        QCOMPARE(i.store.pobBuild(wren).value_or(QString()), ranger({}));

        // and a file too
        i.importer.importText(u"https://pobb.in/AbC123"_s);
        QVERIFY(writeFile(i.dir.filePath(u"Ranger.xml"_s), ranger({}).toUtf8()));
        i.importer.importFile(i.dir.filePath(u"Ranger.xml"_s));
        QVERIFY(!i.importer.isBusy());
        QVERIFY(i.network.sent.last().reply.isNull());
    }

    void fromAFile()
    {
        Importer i;
        const QString path = i.dir.filePath(u"Witch.xml"_s);
        QVERIFY(writeFile(path, pobfixtures::build.toUtf8()));
        i.importer.importFile(path);
        QVERIFY(!i.importer.hasFailed());
        // named after it when you play none
        QCOMPARE(i.store.find(i.list.current())->name, u"Witch"_s);
        QCOMPARE(i.store.pobBuild(i.list.current()).value_or(QString()), pobfixtures::build.trimmed());

        // or with a code in it, by its address
        const QString code = i.dir.filePath(u"code.txt"_s);
        QVERIFY(writeFile(code, codeFor(ranger({})).toLatin1() + "\n"));
        i.importer.importFileUrl(QUrl::fromLocalFile(code));
        QVERIFY(!i.importer.hasFailed());
        QCOMPARE(i.list.rowCount(), 1);
        QCOMPARE(i.importer.message(),
                 u"Imported the level 40 Ranger build for Witch. It has no items on."_s);

        // and nothing for one that's not on this computer
        QSignalSpy message(&i.importer, &BuildImporter::messageChanged);
        i.importer.importFileUrl(QUrl(u"https://example.com/Witch.xml"_s));
        QCOMPARE(message.size(), 0);
    }

    void fileNamedWithSpaces()
    {
        Importer i;
        const QString path = i.dir.filePath(u" .xml"_s);
        QVERIFY(writeFile(path, ranger({}).toUtf8()));
        i.importer.importFile(path);
        QVERIFY2(!i.importer.hasFailed(), qUtf8Printable(i.importer.message()));
        // so it's named after the build
        QCOMPARE(i.store.find(i.list.current())->name, u"Ranger"_s);
    }

    void badFiles()
    {
        Importer i;
        i.importer.importFile(i.dir.filePath(u"Gone.xml"_s));
        QVERIFY(i.importer.hasFailed());
        QVERIFY(i.importer.message().startsWith(u"Couldn't read Gone.xml: "_s));

        QVERIFY(writeFile(i.dir.filePath(u"notes.txt"_s), "Swap to the staff for clearing."));
        i.importer.importFile(i.dir.filePath(u"notes.txt"_s));
        QCOMPARE(i.importer.message(), u"notes.txt isn't a Path of Building build."_s);
        QVERIFY(i.importer.hasFailed());

        QVERIFY(writeFile(i.dir.filePath(u"Old.xml"_s), "<PathOfBuilding><Build/></PathOfBuilding>"));
        i.importer.importFile(i.dir.filePath(u"Old.xml"_s));
        QCOMPARE(i.importer.message(), u"That's a build for Path of Exile 1."_s);

        // which isn't read at all
        QFile huge(i.dir.filePath(u"huge.xml"_s));
        QVERIFY(huge.open(QIODevice::WriteOnly));
        QVERIFY(huge.resize(32 * 1024 * 1024 + 1));
        huge.close();
        i.importer.importFile(huge.fileName());
        QCOMPARE(i.importer.message(), u"huge.xml is too big to be a build."_s);
        QVERIFY(i.importer.hasFailed());
        QCOMPARE(i.list.rowCount(), 0);
    }

    void cantSave()
    {
        Importer i;
        const QString wren = i.play(u"Wren"_s);
        // a folder where the build would be
        QVERIFY(QDir(i.folder).mkdir(wren + u".xml"_s));
        i.importer.importText(ranger({}));
        QVERIFY(i.importer.hasFailed());
        QVERIFY(i.importer.message().startsWith(u"Couldn't save the build for Wren: "_s));
        QCOMPARE(i.importer.message(), i.list.error());
    }

    void cantAdd()
    {
        Importer i;
        // a file where the folder would be
        QVERIFY(writeFile(i.dir.filePath(u"elsewhere"_s), "not a folder"));
        CharacterStore store(i.dir.filePath(u"elsewhere/characters"_s));
        CharacterList list(&store, &i.active, &i.settings);
        BuildImporter importer(&list, &i.engine, &i.network);
        importer.importText(ranger({}));
        QVERIFY(importer.hasFailed());
        QCOMPARE(importer.message(), u"Couldn't save Ranger: can't make the folder %1"_s.arg(
                                         i.dir.filePath(u"elsewhere/characters"_s)));
    }

    void savedBuilds()
    {
        Importer i;
        QCOMPARE(i.importer.buildsFolder(), QString());
        QVERIFY(i.importer.builds().isEmpty());

        // before you've saved any
        QSignalSpy changed(&i.importer, &BuildImporter::buildsChanged);
        i.engine.setInstall({.folder = i.dir.filePath(u"pob"_s), .program = u"luajit"_s});
        QCOMPARE(changed.size(), 1);
        QCOMPARE(i.importer.buildsFolder(), i.builds());
        QVERIFY(i.importer.builds().isEmpty());

        QVERIFY(writeFile(i.builds() + u"/Witch.xml"_s, "<PathOfBuilding2/>"));
        i.importer.refreshBuilds();
        QCOMPARE(changed.size(), 2);
        QCOMPARE(i.importer.builds().size(), 1);
        const QVariantMap witch = i.importer.builds().first().toMap();
        QCOMPARE(witch.value(u"name"_s).toString(), u"Witch"_s);
        QCOMPARE(witch.value(u"path"_s).toString(), i.builds() + u"/Witch.xml"_s);
        QCOMPARE(witch.value(u"modified"_s).toDateTime(),
                 QFileInfo(i.builds() + u"/Witch.xml"_s).lastModified());

        // the same again says nothing
        i.importer.refreshBuilds();
        QCOMPARE(changed.size(), 2);
        QVERIFY(writeFile(i.builds() + u"/Leveling/Monk.xml"_s, "<PathOfBuilding2/>"));
        i.importer.refreshBuilds();
        QCOMPARE(changed.size(), 3);
        QCOMPARE(i.importer.builds().size(), 2);

        i.engine.setUnavailable(u"Path of Building isn't installed."_s);
        QCOMPARE(changed.size(), 4);
        QCOMPARE(i.importer.buildsFolder(), QString());
        QVERIFY(i.importer.builds().isEmpty());
    }

    void savedBuildsFromTheStart()
    {
        Importer i;
        i.engine.setInstall({.folder = i.dir.filePath(u"pob"_s), .program = u"luajit"_s});
        QVERIFY(writeFile(i.builds() + u"/Witch.xml"_s, "<PathOfBuilding2/>"));
        BuildImporter later(&i.list, &i.engine, &i.network);
        QCOMPARE(later.buildsFolder(), i.builds());
        QCOMPARE(later.builds().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestBuildImporter)
#include "tst_buildimporter.moc"
