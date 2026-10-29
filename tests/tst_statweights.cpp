// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/statweights.h"
#include "core/parser.h"
#include "services/pobengine.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <limits>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// The weights of the character you play, as the window shows them.
struct Weighing
{
    QTemporaryDir dir;
    CharacterStore store{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    StatWeights weights{&active};
    QAbstractItemModelTester tester{&weights, QAbstractItemModelTester::FailureReportingMode::QtTest};

    QString play(const Character &character)
    {
        const QString id = store.add(character).value_or(QString());
        active.setId(id);
        return id;
    }

    QStringList keys() const
    {
        QStringList keys;
        for (int row = 0; row < weights.rowCount(); ++row)
            keys << weights.index(row).data(StatWeights::KeyRole).toString();
        return keys;
    }

    QVariant of(const QString &key, StatWeights::Role role) const
    {
        const auto row = keys().indexOf(key);
        return row < 0 ? QVariant() : weights.index(int(row)).data(role);
    }

    const WeightSuggestion *suggestion(const QString &key) const
    {
        for (const WeightSuggestion &s : weights.suggestions()) {
            if (s.key == key)
                return &s;
        }
        return nullptr;
    }

    const Weights &saved() const { return store.find(active.id())->weights; }
};

const QString life = u"# to maximum Life"_s;
const QString intelligence = u"# to Intelligence"_s;
const QString rarity = u"#% increased Rarity of Items found"_s;
const QString armour = u"[Base] Armour"_s;

Character tester(const Weights &weights = defaultWeights())
{
    Character c;
    c.name = u"Tester"_s;
    c.weights = weights;
    return c;
}

Item item(const QString &text)
{
    return parseItem(text).value();
}

// life on both, and intelligence and rarity on one each
Equipped someGear()
{
    return {
        {u"Helmet"_s, item(u"Item Class: Helmets\nRarity: Rare\nGrim Crown\nIron Hat\n--------\nArmour: 100\n"
                           "--------\n+40 to maximum Life\n+20 to Intelligence"_s)},
        {u"Boots"_s, item(u"Item Class: Boots\nRarity: Rare\nMud Stride\nIron Greaves\n--------\nArmour: 80\n"
                          "--------\n+60 to maximum Life\n10% increased Rarity of Items found"_s)},
    };
}

Character wearing(const Equipped &gear, const Weights &weights = defaultWeights())
{
    Character c = tester(weights);
    c.gear = gear;
    return c;
}

} // namespace

class TestStatWeights : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nobody()
    {
        Weighing w;
        QCOMPARE(w.weights.rowCount(), 0);
        QCOMPARE(w.weights.character(), QString());
        QVERIFY(w.weights.suggestions().isEmpty());
        QVERIFY(!w.weights.suggestsNew());
        QCOMPARE(w.weights.undoable(), QString());

        QSignalSpy dataChanged(&w.weights, &StatWeights::dataChanged);
        w.active.reload();
        QCOMPARE(dataChanged.size(), 0);
    }

    void rows()
    {
        Weighing w;
        QSignalSpy changed(&w.weights, &StatWeights::changed);
        w.play(tester({{life, 1},
                       {armour, 0.05},
                       {u"#% to Fire Resistance"_s, 1.5},
                       {u"# to Dexterity"_s, 0.4},
                       {u"[Weapon] Total DPS"_s, 1}}));
        QCOMPARE(w.weights.character(), u"Tester"_s);
        // the item's own first
        QCOMPARE(w.keys(), QStringList({armour, u"[Weapon] Total DPS"_s, u"# to Dexterity"_s, life,
                                        u"#% to Fire Resistance"_s}));
        QCOMPARE(w.of(armour, StatWeights::NameRole).toString(), u"Armour"_s);
        QCOMPARE(w.of(armour, StatWeights::NoteRole).toString(), u"The item's own, with its mods"_s);
        QCOMPARE(w.of(armour, StatWeights::WeightRole).toDouble(), 0.05);
        QCOMPARE(w.of(u"[Weapon] Total DPS"_s, StatWeights::NoteRole).toString(),
                 u"The weapon's own, with its mods"_s);
        QCOMPARE(w.of(life, StatWeights::NameRole).toString(), life);
        QCOMPARE(w.of(life, StatWeights::NoteRole).toString(), QString());
        QVERIFY(!w.weights.isDefault());
        QCOMPARE(changed.size(), 1);
    }

    void defaults()
    {
        Weighing w;
        w.play(tester());
        QVERIFY(w.weights.isDefault());
        QCOMPARE(w.weights.rowCount(), defaultWeights().size());
    }

    void ignored()
    {
        Weighing w;
        Character c = tester();
        c.ignored = {{.preset = {}, .key = life, .mode = IgnoreRule::Mode::Ignore}};
        w.play(c);
        QCOMPARE(w.of(life, StatWeights::NoteRole).toString(), u"You ignore it, so it doesn't count"_s);
    }

    void ignoredLater()
    {
        Weighing w;
        const QString id = w.play(tester());
        Character c = *w.store.find(id);
        c.ignored = {{.preset = {}, .key = life, .mode = IgnoreRule::Mode::Ignore}};
        QVERIFY(w.store.save(id, c));
        QSignalSpy dataChanged(&w.weights, &StatWeights::dataChanged);
        w.active.reload();
        QCOMPARE(dataChanged.size(), 1);
        QVERIFY(dataChanged.at(0).at(2).value<QList<int>>().contains(StatWeights::NoteRole));
        QCOMPARE(w.of(life, StatWeights::NoteRole).toString(), u"You ignore it, so it doesn't count"_s);
    }

    void setWeight()
    {
        Weighing w;
        w.play(tester());
        QSignalSpy reset(&w.weights, &StatWeights::modelReset);
        QSignalSpy dataChanged(&w.weights, &StatWeights::dataChanged);
        QSignalSpy errorChanged(&w.weights, &StatWeights::errorChanged);
        w.weights.setWeight(life, 2.5);
        QCOMPARE(w.saved().value(life), 2.5);
        QCOMPARE(w.of(life, StatWeights::WeightRole).toDouble(), 2.5);
        QCOMPARE(dataChanged.size(), 1);
        QCOMPARE(reset.size(), 0);

        // nothing and less than nothing are weights too
        w.weights.setWeight(life, 0);
        QVERIFY(w.saved().contains(life));
        w.weights.setWeight(life, -1);
        QCOMPARE(w.saved().value(life), -1.0);
        QCOMPARE(errorChanged.size(), 0);
    }

    void wrongWeights()
    {
        Weighing w;
        w.play(tester());
        QSignalSpy changed(&w.active, &ActiveCharacter::changed);
        w.weights.setWeight(life, 1);
        w.weights.setWeight(life, std::numeric_limits<double>::quiet_NaN());
        w.weights.setWeight(life, std::numeric_limits<double>::infinity());
        // one without a weight
        w.weights.setWeight(rarity, 2);
        QCOMPARE(changed.size(), 0);
    }

    void addAndRemove()
    {
        Weighing w;
        w.play(tester({{life, 2}}));
        QSignalSpy reset(&w.weights, &StatWeights::modelReset);
        w.weights.add(rarity);
        QCOMPARE(w.saved(), Weights({{life, 2}, {rarity, 1}}));
        QCOMPARE(w.keys(), QStringList({life, rarity}));
        QCOMPARE(reset.size(), 1);

        QSignalSpy changed(&w.active, &ActiveCharacter::changed);
        w.weights.add(life);
        w.weights.add(QString());
        w.weights.remove(intelligence);
        QCOMPARE(changed.size(), 0);
        QCOMPARE(w.saved().value(life), 2.0);

        w.weights.remove(life);
        QCOMPARE(w.saved(), Weights({{rarity, 1}}));
        QCOMPARE(w.keys(), QStringList({rarity}));
    }

    void resetAndUndo()
    {
        Weighing w;
        w.play(tester({{life, 2}}));
        QStringList seen;
        connect(&w.weights, &StatWeights::changed, this, [&] { seen << w.weights.undoable(); });
        w.weights.reset();
        QCOMPARE(w.saved(), defaultWeights());
        QVERIFY(w.weights.isDefault());
        QCOMPARE(w.weights.undoable(), u"Your weights are back to the defaults."_s);
        QCOMPARE(seen.constLast(), w.weights.undoable());

        w.weights.undo();
        QCOMPARE(w.saved(), Weights({{life, 2}}));
        QCOMPARE(w.weights.undoable(), QString());

        // and with nothing to undo
        QSignalSpy changed(&w.active, &ActiveCharacter::changed);
        w.weights.undo();
        QCOMPARE(changed.size(), 0);
    }

    void resetDefaults()
    {
        Weighing w;
        w.play(tester());
        QSignalSpy changed(&w.active, &ActiveCharacter::changed);
        w.weights.reset();
        QCOMPARE(changed.size(), 0);
        QCOMPARE(w.weights.undoable(), QString());
    }

    void undoGoesWithTheNextChange()
    {
        Weighing w;
        w.play(tester({{life, 2}}));
        w.weights.reset();
        w.weights.setWeight(life, 3);
        QCOMPARE(w.weights.undoable(), QString());
        w.weights.undo();
        QCOMPARE(w.saved().value(life), 3.0);
    }

    void undoStaysWithTheCharacter()
    {
        Weighing w;
        const QString first = w.play(tester({{life, 2}}));
        w.weights.reset();
        Character other = tester();
        other.name = u"Ottoline"_s;
        w.play(other);
        QCOMPARE(w.weights.undoable(), QString());

        w.active.setId(first);
        QCOMPARE(w.weights.undoable(), QString());
    }

    void undoGoesWithTheCharacter()
    {
        Weighing w;
        const QString id = w.play(tester({{life, 2}}));
        w.weights.reset();
        QVERIFY(w.store.remove(id));
        w.active.reload();
        QCOMPARE(w.weights.undoable(), QString());
    }

    void suggestions()
    {
        Weighing w;
        w.play(wearing(someGear()));
        const WeightSuggestion *forLife = w.suggestion(life);
        QVERIFY(forLife);
        QCOMPARE(forLife->name, life);
        QCOMPARE(forLife->weight, 0.9);
        QCOMPARE(forLife->why, u"On 2 of your items, 50 on average. Your weight for it is 1"_s);
        QCOMPARE(w.suggestion(rarity)->why, u"On one of your items, at 10"_s);
        QCOMPARE(w.suggestion(rarity)->weight, 3.0);
        QCOMPARE(w.suggestion(armour)->name, u"Armour"_s);
        const QList<WeightSuggestion> all = w.weights.suggestions();
        QCOMPARE(all.size(), 4);
        // the stats on the most items first
        QCOMPARE(QSet<QString>({all.at(0).key, all.at(1).key}), QSet<QString>({life, armour}));
        QVERIFY(w.weights.suggestsNew());
    }

    void ignoredArentSuggested()
    {
        Weighing w;
        Character c = wearing(someGear());
        c.ignored = {{.preset = {}, .key = rarity, .mode = IgnoreRule::Mode::Ignore}};
        w.play(c);
        QVERIFY(w.suggestion(life));
        QVERIFY(!w.suggestion(rarity));
    }

    void useSuggestions()
    {
        Weighing w;
        w.play(wearing(someGear()));
        w.weights.useSuggestions();
        QCOMPARE(w.saved(), Weights({{life, 0.9}, {armour, 0.44}, {intelligence, 1.5}, {rarity, 3}}));
        QCOMPARE(w.weights.undoable(), u"Your weights are the suggested ones now."_s);
        QVERIFY(!w.weights.suggestsNew());

        // which they are already
        QSignalSpy changed(&w.active, &ActiveCharacter::changed);
        w.weights.useSuggestions();
        QCOMPARE(changed.size(), 0);

        w.weights.undo();
        QCOMPARE(w.saved(), defaultWeights());
    }

    void nothingToSuggest()
    {
        Weighing w;
        w.play(tester({{life, 2}}));
        QSignalSpy changed(&w.active, &ActiveCharacter::changed);
        w.weights.useSuggestions();
        w.weights.addSuggestions();
        QCOMPARE(changed.size(), 0);
    }

    void addSuggestions()
    {
        Weighing w;
        w.play(wearing(someGear(), {{life, 2}, {intelligence, 1}}));
        w.weights.addSuggestions();
        QCOMPARE(w.saved(), Weights({{life, 2}, {intelligence, 1}, {armour, 0.44}, {rarity, 3}}));
        QCOMPARE(w.weights.undoable(), u"Added 2 suggested weights to yours."_s);
        QVERIFY(!w.weights.suggestsNew());

        QSignalSpy changed(&w.active, &ActiveCharacter::changed);
        w.weights.addSuggestions();
        QCOMPARE(changed.size(), 0);

        w.weights.undo();
        QCOMPARE(w.saved(), Weights({{life, 2}, {intelligence, 1}}));
    }

    void addOneSuggestion()
    {
        Weighing w;
        w.play(wearing(someGear(), {{life, 2}, {intelligence, 1}, {armour, 1}}));
        w.weights.addSuggestions();
        QCOMPARE(w.weights.undoable(), u"Added a suggested weight to yours."_s);
    }

    void cantSave()
    {
        Weighing w;
        const QString id = w.play(tester({{life, 2}}));
        w.weights.reset();
        // where a folder is in the way
        const QString file = w.dir.filePath(u"characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QSignalSpy errorChanged(&w.weights, &StatWeights::errorChanged);
        w.weights.setWeight(life, 3);
        QVERIFY(w.weights.error().startsWith(u"Couldn't save Tester: "_s));
        QCOMPARE(errorChanged.size(), 1);
        // as nothing changed
        QCOMPARE(w.weights.undoable(), u"Your weights are back to the defaults."_s);

        QVERIFY(QDir().rmdir(file));
        w.weights.undo();
        QCOMPARE(w.weights.error(), QString());
        QCOMPARE(errorChanged.size(), 2);
        QCOMPARE(w.saved(), Weights({{life, 2}}));
    }

    void cantSaveTheSuggestions()
    {
        Weighing w;
        const QString id = w.play(wearing(someGear()));
        const QString file = w.dir.filePath(u"characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        w.weights.useSuggestions();
        QVERIFY(!w.weights.error().isEmpty());
        QCOMPARE(w.weights.undoable(), QString());
    }

    void anotherCharacter()
    {
        Weighing w;
        w.play(wearing(someGear()));
        Character other = tester({{rarity, 1}});
        other.name = u"Ottoline"_s;
        w.play(other);
        QCOMPARE(w.weights.character(), u"Ottoline"_s);
        QCOMPARE(w.keys(), QStringList({rarity}));
        QVERIFY(w.weights.suggestions().isEmpty());

        // and nobody
        w.active.setId(u"gone"_s);
        QCOMPARE(w.weights.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(TestStatWeights)
#include "tst_statweights.moc"
