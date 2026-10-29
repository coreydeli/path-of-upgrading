// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "core/parser.h"
#include "fakepob.h"
#include "itemtexts.h"
#include "services/pobengine.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/></PathOfBuilding2>"_s;

Item item(const QString &text)
{
    return parseItem(text).value();
}

Character inGrimCrown()
{
    Character c;
    c.name = u"Torvald"_s;
    c.gear = {{u"Helmet"_s, item(itemtexts::helmetOld)}};
    return c;
}

// What Path of Building has, by what the fake says when it's asked about an
// item.
QJsonObject pobHas(PobEngine &engine)
{
    const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = itemtexts::helmetNew}}));
    QSignalSpy finished(reply.get(), &PobReply::finished);
    if (!reply->isFinished() && !finished.wait())
        return {};
    if (!reply->errorString().isEmpty())
        return {{u"error"_s, reply->errorString()}};
    return reply->answer();
}

QJsonArray wearing(const Item &item, const QString &slot)
{
    return {QJsonObject{{u"text"_s, item.raw}, {u"slot"_s, slot}}};
}

// Your characters with Path of Building, which hasn't started.
struct Characters
{
    QTemporaryDir dir;
    CharacterStore store{dir.path()};
    FakePob pob;
    PobEngine engine;
    ActiveCharacter active{&store, &engine};

    Characters() { engine.setInstall(pob.install()); }

    // a new one, with the build when there is one
    QString add(const Character &character, const QString &xml = {})
    {
        const QString id = store.add(character).value_or(QString());
        if (!xml.isEmpty())
            store.savePobBuild(id, xml);
        return id;
    }

    // what's saved for it, once the store is read again
    Character saved(const QString &id) const
    {
        CharacterStore again(dir.path());
        again.load();
        const Character *character = again.find(id);
        return character ? *character : Character();
    }

    // once Path of Building has loaded the build and found Chaos Inoculation,
    // or not
    bool loads(bool chaosInoculation)
    {
        pob.loads({{u"stats"_s, QJsonObject{{u"ChaosInoculation"_s, chaosInoculation ? 1 : 0}}}});
        QSignalSpy loaded(&engine, &PobEngine::buildLoaded);
        engine.stop();
        pobHas(engine);
        return loaded.size() == 1;
    }
};

} // namespace

class TestActiveCharacter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void itsBuild()
    {
        Characters c;
        Character torvald = inGrimCrown();
        // and a ring that came with the build, which Path of Building has
        torvald.gear.insert(u"Ring 1"_s, item(itemtexts::advancedRing));
        torvald.fromPob = {u"Ring 1"_s};
        const QString id = c.add(torvald, build);
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        c.active.setId(id);
        QCOMPARE(c.active.id(), id);
        QVERIFY(c.active.exists());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(c.active.character().name, u"Torvald"_s);
        QVERIFY(c.active.build());
        QCOMPARE(c.active.build()->className, u"Warrior"_s);

        const QJsonObject has = pobHas(c.engine);
        QCOMPARE(has.value(u"xml").toString(), build);
        QCOMPARE(has.value(u"wearing").toArray(), wearing(item(itemtexts::helmetOld), u"Helmet"_s));
    }

    void weaponSwap()
    {
        // with Path of Building's second weapon set in use
        const QString swapped = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/>"
                                u"<Items activeItemSet=\"1\"><ItemSet id=\"1\" useSecondWeaponSet=\"true\"/>"
                                u"</Items></PathOfBuilding2>"_s;
        Characters c;
        Character torvald;
        torvald.gear = {{u"Weapon"_s, item(itemtexts::twoHandMace)}};
        c.active.setId(c.add(torvald, swapped));
        QCOMPARE(pobHas(c.engine).value(u"wearing").toArray(),
                 wearing(item(itemtexts::twoHandMace), u"Weapon 1 Swap"_s));
    }

    void noBuild()
    {
        Characters c;
        c.active.setId(c.add(inGrimCrown()));
        QCOMPARE(c.active.character().name, u"Torvald"_s);
        QVERIFY(!c.active.build());
        QCOMPARE(pobHas(c.engine).value(u"error").toString(), u"There's no build to judge items with."_s);
    }

    void notForPoe2()
    {
        Characters c;
        c.active.setId(c.add(inGrimCrown(), u"<PathOfBuilding><Build level=\"90\"/></PathOfBuilding>"_s));
        QVERIFY(!c.active.build());
        QCOMPARE(pobHas(c.engine).value(u"error").toString(), u"There's no build to judge items with."_s);
    }

    void nobody()
    {
        // once you played one with a build
        Characters c;
        c.active.setId(c.add(inGrimCrown(), build));
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        c.active.setId(u"nobody"_s);
        QVERIFY(!c.active.exists());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(c.active.character().name, QString());
        QVERIFY(c.active.character().gear.isEmpty());
        QVERIFY(!c.active.build());
        QCOMPARE(pobHas(c.engine).value(u"error").toString(), u"There's no build to judge items with."_s);
    }

    void reload()
    {
        Characters c;
        const QString id = c.add(inGrimCrown(), build);
        c.active.setId(id);
        Character torvald = inGrimCrown();
        torvald.gear.insert(u"Helmet"_s, item(itemtexts::helmetNew));
        QVERIFY(c.store.save(id, torvald));
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        c.active.reload();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(c.active.character().gear.value(u"Helmet"_s).name, u"Doom Visage"_s);
        QCOMPARE(pobHas(c.engine).value(u"wearing").toArray(),
                 wearing(item(itemtexts::helmetNew), u"Helmet"_s));
    }

    void chaosInoculation()
    {
        Characters c;
        const QString id = c.add(inGrimCrown(), build);
        c.active.setId(id);
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        QVERIFY(c.loads(true));
        QCOMPARE(changed.size(), 1);
        QVERIFY(c.active.character().chaosInoculation);
        QVERIFY(c.saved(id).chaosInoculation);

        // and not again when it's the same
        QVERIFY(c.loads(true));
        QCOMPARE(changed.size(), 1);

        // and once it's gone
        QVERIFY(c.loads(false));
        QCOMPARE(changed.size(), 2);
        QVERIFY(!c.active.character().chaosInoculation);
        QVERIFY(!c.saved(id).chaosInoculation);
    }

    void keepsYourChanges()
    {
        // made since it was read
        Characters c;
        const QString id = c.add(inGrimCrown(), build);
        c.active.setId(id);
        Character renamed = inGrimCrown();
        renamed.name = u"Torvald the Grim"_s;
        QVERIFY(c.store.save(id, renamed));
        QVERIFY(c.loads(true));
        QCOMPARE(c.saved(id).name, u"Torvald the Grim"_s);
        QVERIFY(c.saved(id).chaosInoculation);
    }

    void cantSave()
    {
        Characters c;
        const QString id = c.add(inGrimCrown(), build);
        c.active.setId(id);
        // where a folder is in the way
        QVERIFY(QFile::remove(c.dir.filePath(id + u".json"_s)));
        QVERIFY(QDir(c.dir.path()).mkdir(id + u".json"_s));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(u"^Couldn't save what Path of Building found out about"_s));
        QVERIFY(c.loads(true));
        // and it's used anyway
        QVERIFY(c.active.character().chaosInoculation);
    }

    void wear()
    {
        Characters c;
        Character torvald = inGrimCrown();
        torvald.fromPob = {u"Helmet"_s};
        const QString id = c.add(torvald, build);
        c.active.setId(id);
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        QVERIFY(c.active.wear(u"Helmet"_s, item(itemtexts::helmetNew)));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(c.active.character().gear.value(u"Helmet"_s).name, u"Doom Visage"_s);
        QVERIFY(c.active.character().fromPob.isEmpty());
        QCOMPARE(c.saved(id).gear.value(u"Helmet"_s).name, u"Doom Visage"_s);
        QVERIFY(c.saved(id).fromPob.isEmpty());
        // and Path of Building has it on
        QCOMPARE(pobHas(c.engine).value(u"wearing").toArray(),
                 wearing(item(itemtexts::helmetNew), u"Helmet"_s));
    }

    void wearElsewhere()
    {
        Characters c;
        const QString id = c.add(inGrimCrown());
        c.active.setId(id);
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        QVERIFY(!c.active.wear(u"Ring 1"_s, item(itemtexts::helmetNew)));
        QCOMPARE(c.active.errorString(), u"Doom Visage can't go in Ring 1."_s);
        QCOMPARE(changed.size(), 0);
        QCOMPARE(c.saved(id).gear.value(u"Helmet"_s).name, u"Grim Crown"_s);

        // which is forgotten once it's worn
        QVERIFY(c.active.wear(u"Ring 1"_s, item(itemtexts::advancedRing)));
        QCOMPARE(c.active.errorString(), QString());
    }

    void wearForNobody()
    {
        Characters c;
        QVERIFY(!c.active.wear(u"Helmet"_s, item(itemtexts::helmetNew)));
        QCOMPARE(c.active.errorString(), u"There's no character to wear it. Add one first."_s);
        QVERIFY(c.active.takeOff(u"Helmet"_s));
    }

    void takeOff()
    {
        Characters c;
        Character torvald = inGrimCrown();
        torvald.gear.insert(u"Ring 1"_s, item(itemtexts::advancedRing));
        torvald.fromPob = {u"Ring 1"_s};
        const QString id = c.add(torvald, build);
        c.active.setId(id);
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        QVERIFY(c.active.takeOff(u"Ring 1"_s));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(c.active.character().gear.keys(), QStringList{u"Helmet"_s});
        QVERIFY(c.active.character().fromPob.isEmpty());
        QCOMPARE(c.saved(id).gear.keys(), QStringList{u"Helmet"_s});
        QCOMPARE(pobHas(c.engine).value(u"wearing").toArray(),
                 wearing(item(itemtexts::helmetOld), u"Helmet"_s));

        // nothing's there now
        QVERIFY(c.active.takeOff(u"Ring 1"_s));
        QCOMPARE(changed.size(), 1);
    }

    void cantWear()
    {
        Characters c;
        const QString id = c.add(inGrimCrown(), build);
        c.active.setId(id);
        QVERIFY(QFile::remove(c.dir.filePath(id + u".json"_s)));
        QVERIFY(QDir(c.dir.path()).mkdir(id + u".json"_s));
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        QVERIFY(!c.active.wear(u"Helmet"_s, item(itemtexts::helmetNew)));
        QVERIFY(c.active.errorString().startsWith(u"Couldn't save Torvald: "_s));
        QVERIFY(!c.active.takeOff(u"Helmet"_s));
        QCOMPARE(changed.size(), 0);
        QCOMPARE(c.active.character().gear.value(u"Helmet"_s).name, u"Grim Crown"_s);
    }

    void change()
    {
        Characters c;
        const QString id = c.add(inGrimCrown(), build);
        c.active.setId(id);
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        QVERIFY(c.active.change([](Character &torvald) { torvald.buildProfile = u"es-caster"_s; }));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(c.active.character().buildProfile, u"es-caster"_s);
        QCOMPARE(c.saved(id).buildProfile, u"es-caster"_s);
        QCOMPARE(c.active.errorString(), QString());
    }

    void cantChange()
    {
        Characters c;
        QVERIFY(!c.active.change([](Character &) {}));
        QCOMPARE(c.active.errorString(), u"There's no character to change. Add one first."_s);

        const QString id = c.add(inGrimCrown(), build);
        c.active.setId(id);
        QVERIFY(QFile::remove(c.dir.filePath(id + u".json"_s)));
        QVERIFY(QDir(c.dir.path()).mkdir(id + u".json"_s));
        QSignalSpy changed(&c.active, &ActiveCharacter::changed);
        QVERIFY(!c.active.change([](Character &torvald) { torvald.buildProfile = u"es-caster"_s; }));
        QVERIFY(c.active.errorString().startsWith(u"Couldn't save Torvald: "_s));
        QCOMPARE(changed.size(), 0);
        QCOMPARE(c.active.character().buildProfile, QString());
    }

    void removed()
    {
        // before it's read again
        Characters c;
        const QString id = c.add(inGrimCrown(), build);
        c.active.setId(id);
        QVERIFY(c.store.remove(id));
        QVERIFY(c.loads(true));
        QVERIFY(!c.active.character().chaosInoculation);
        QVERIFY(!QFile::exists(c.dir.filePath(id + u".json"_s)));
    }
};

QTEST_GUILESS_MAIN(TestActiveCharacter)
#include "tst_activecharacter.moc"
