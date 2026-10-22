// SPDX-License-Identifier: MIT
#include "core/characterstore.h"
#include "core/parser.h"
#include "itemtexts.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

} // namespace

class TestCharacterStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip()
    {
        Character ember;
        ember.name = u"Ember"_s;
        ember.gear = {{u"Helmet"_s, item(itemtexts::helmetNew)},
                      {u"Ring 1"_s, item(itemtexts::advancedRing)}};
        ember.fromPob = {u"Ring 1"_s};
        ember.weights = {{u"# to maximum Life"_s, 1.5}, {u"#% to Fire Resistance"_s, 1}};
        ember.ignored = {{u"chaosres"_s, {}, IgnoreRule::Mode::Exclude}};
        ember.buildProfile = u"minion-es"_s;
        ember.tweaks = {{u"Spirit"_s, 2}};
        ResistEntry entry;
        entry.values[Element::Fire] = 78;
        entry.values[Element::Chaos] = -12;
        ember.resists = snapshotResists(entry, ember.gear, ResistSource::Pob);
        ember.chaosInoculation = true;

        const QJsonObject saved = toJson(ember);
        QCOMPARE(saved.value(u"version").toInt(), 1);
        QCOMPARE(saved.value(u"gear").toObject().value(u"Ring 1").toString(), itemtexts::advancedRing);

        const Character back = characterFromJson(saved).value();
        QCOMPARE(back.name, ember.name);
        QCOMPARE(back.gear.size(), 2);
        QCOMPARE(back.gear.value(u"Helmet"_s).name, ember.gear.value(u"Helmet"_s).name);
        QVERIFY(back.gear.value(u"Ring 1"_s).affixes.known);
        QCOMPARE(back.fromPob, ember.fromPob);
        QCOMPARE(back.weights, ember.weights);
        QCOMPARE(back.ignored, ember.ignored);
        QCOMPARE(back.buildProfile, ember.buildProfile);
        QCOMPARE(back.tweaks, ember.tweaks);
        QCOMPARE(back.resists, ember.resists);
        QVERIFY(back.chaosInoculation);
    }

    void nothingSaved()
    {
        const Character fresh = characterFromJson({}).value();
        QVERIFY(fresh.gear.isEmpty());
        QCOMPARE(fresh.weights, defaultWeights());
        QVERIFY(!fresh.resists);
        QVERIFY(!fresh.chaosInoculation);
        QVERIFY(!toJson(fresh).contains(u"resists"));
    }

    void noWeightsAtAll()
    {
        const Character bare = characterFromJson({{u"weights"_s, QJsonObject()}}).value();
        QVERIFY(bare.weights.isEmpty());
    }

    void gearItCantRead()
    {
        const QJsonObject saved = {
            {u"gear"_s, QJsonObject{{u"Helmet"_s, itemtexts::helmetNew}, {u"Gloves"_s, u"Rarity: Rare"_s}}},
            {u"fromPob"_s, QJsonArray{u"Helmet"_s, u"Gloves"_s, u"Boots"_s}},
        };
        const Character c = characterFromJson(saved).value();
        QCOMPARE(c.gear.keys(), QStringList{u"Helmet"_s});
        QCOMPARE(c.fromPob, QSet<QString>{u"Helmet"_s});
    }

    void savedByANewerVersion()
    {
        const QJsonObject saved = {{u"version"_s, 2}, {u"name"_s, u"Ember"_s}};
        QVERIFY(!characterFromJson(saved));
    }

    void keptForNextTime()
    {
        QTemporaryDir dir;
        CharacterStore store(dir.path());
        Character frost;
        frost.name = u"Frost"_s;
        frost.gear = {{u"Helmet"_s, item(itemtexts::helmetNew)}};
        const QString frostId = store.add(frost).value();
        Character ember;
        ember.name = u"ember"_s;
        const QString emberId = store.add(ember).value();
        QCOMPARE(store.ids(), (QStringList{emberId, frostId}));

        CharacterStore again(dir.path());
        again.load();
        QCOMPARE(again.ids(), (QStringList{emberId, frostId}));
        QCOMPARE(again.find(frostId)->gear.value(u"Helmet"_s).raw, itemtexts::helmetNew);
        QVERIFY(again.problems().isEmpty());
        QVERIFY(!again.find(u"someone-else"_s));
    }

    void changes()
    {
        QTemporaryDir dir;
        CharacterStore store(dir.path());
        const QString id = store.add({}).value();
        Character renamed = *store.find(id);
        renamed.name = u"Ash"_s;
        QVERIFY(store.save(id, renamed));
        QCOMPARE(store.find(id)->name, u"Ash"_s);

        CharacterStore again(dir.path());
        again.load();
        QCOMPARE(again.find(id)->name, u"Ash"_s);

        QVERIFY(!store.save(u"someone-else"_s, renamed));
        QVERIFY(!store.errorString().isEmpty());
        QCOMPARE(store.ids(), QStringList{id});
    }

    void builds()
    {
        QTemporaryDir dir;
        CharacterStore store(dir.path());
        const QString id = store.add({}).value();
        QVERIFY(!store.pobBuild(id));
        const QString xml = u"<PathOfBuilding2>\n</PathOfBuilding2>\n"_s;
        QVERIFY(store.savePobBuild(id, xml));
        QCOMPARE(store.pobBuild(id).value(), xml);
        QVERIFY(!store.savePobBuild(u"someone-else"_s, xml));
    }

    void removing()
    {
        QTemporaryDir dir;
        CharacterStore store(dir.path());
        const QString id = store.add({}).value();
        QVERIFY(store.savePobBuild(id, u"<PathOfBuilding2/>"_s));
        QVERIFY(store.remove(id));
        QVERIFY(store.ids().isEmpty());
        QVERIFY(QDir(dir.path()).isEmpty());
        QVERIFY(!store.remove(id));
    }

    void filesItCantRead()
    {
        QTemporaryDir dir;
        CharacterStore store(dir.path());
        const QString id = store.add({}).value();
        const auto writeRaw = [&](const QString &name, const QByteArray &data) {
            QFile file(dir.filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(writeRaw(u"broken.json"_s, "{\"name\": \"Em"));
        QVERIFY(writeRaw(u"future.json"_s, "{\"version\": 2}"));

        store.load();
        QCOMPARE(store.ids(), QStringList{id});
        QCOMPARE(store.problems().size(), 2);
        QVERIFY(store.problems().at(0).startsWith(u"broken.json: "));
        QCOMPARE(store.problems().at(1), u"future.json: saved by a newer version of Path of Upgrading"_s);
        // left for you to look at
        QVERIFY(QFile::exists(dir.filePath(u"broken.json"_s)));
    }
};

QTEST_GUILESS_MAIN(TestCharacterStore)
#include "tst_characterstore.moc"
