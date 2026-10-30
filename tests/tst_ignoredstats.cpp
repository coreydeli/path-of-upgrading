// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/ignoredstats.h"
#include "services/pobengine.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString life = u"# to maximum Life"_s;
const QString fire = u"#% to Fire Resistance"_s;
const QString rarity = u"#% increased Rarity of Items found"_s;
const QString armour = u"[Base] Armour"_s;

const QStringList groupIds = {u"res"_s, u"eleres"_s, u"chaosres"_s, u"attributes"_s, u"life"_s, u"mana"_s};

IgnoreRule single(const QString &key, IgnoreRule::Mode mode = IgnoreRule::Mode::Ignore)
{
    return {.preset = {}, .key = key, .mode = mode};
}

IgnoreRule group(const QString &id, IgnoreRule::Mode mode = IgnoreRule::Mode::Ignore)
{
    return {.preset = id, .key = {}, .mode = mode};
}

Character tester(const QList<IgnoreRule> &ignored = {})
{
    Character c;
    c.name = u"Tester"_s;
    c.ignored = ignored;
    return c;
}

// What the character you play ignores, as the window shows it.
struct Ignoring
{
    QTemporaryDir dir;
    CharacterStore store{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    IgnoredStats ignored{&active};
    QAbstractItemModelTester tester{&ignored, QAbstractItemModelTester::FailureReportingMode::QtTest};

    QString play(const Character &character)
    {
        const QString id = store.add(character).value_or(QString());
        active.setId(id);
        return id;
    }

    QStringList keys() const
    {
        QStringList keys;
        for (int row = 0; row < ignored.rowCount(); ++row)
            keys << ignored.index(row).data(IgnoredStats::KeyRole).toString();
        return keys;
    }

    QVariant of(const QString &key, IgnoredStats::Role role) const
    {
        const auto row = keys().indexOf(key);
        return row < 0 ? QVariant() : ignored.index(int(row)).data(role);
    }

    int groupMode(const QString &id) const
    {
        for (const IgnoredGroup &g : ignored.groups()) {
            if (g.id == id)
                return g.mode;
        }
        return -1;
    }

    QList<IgnoreRule> saved() const { return store.find(active.id())->ignored; }
};

} // namespace

class TestIgnoredStats : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nobody()
    {
        Ignoring i;
        QCOMPARE(i.ignored.rowCount(), 0);
        QCOMPARE(i.ignored.character(), QString());
        QVERIFY(i.ignored.groups().isEmpty());
        QVERIFY(i.ignored.keys().isEmpty());
        QCOMPARE(i.ignored.modes(), QStringList({u"Counts"_s, u"Ignored"_s, u"Excluded"_s}));

        // and there's no one to change
        i.ignored.add(life);
        QCOMPARE(i.ignored.error(), QString());
    }

    void groups()
    {
        Ignoring i;
        i.play(tester({group(u"eleres"_s, IgnoreRule::Mode::Exclude), group(u"mana"_s)}));
        QCOMPARE(i.ignored.character(), u"Tester"_s);
        QStringList ids;
        for (const IgnoredGroup &g : i.ignored.groups())
            ids << g.id;
        QCOMPARE(ids, groupIds);
        const IgnoredGroup eleres = i.ignored.groups().at(1);
        QCOMPARE(eleres.name, u"Elemental resistances"_s);
        QCOMPARE(eleres.stats, u"#% to Fire Resistance, #% to Cold Resistance, #% to Lightning Resistance"_s);
        QCOMPARE(eleres.mode, int(IgnoredStats::Excluded));
        QCOMPARE(i.groupMode(u"mana"_s), int(IgnoredStats::Ignored));
        QCOMPARE(i.groupMode(u"res"_s), int(IgnoredStats::Counts));
        // no single stats
        QCOMPARE(i.ignored.rowCount(), 0);
        QCOMPARE(i.ignored.keys(), QStringList({fire, u"#% to Cold Resistance"_s,
                                                u"#% to Lightning Resistance"_s, u"# to maximum Mana"_s}));
    }

    void rows()
    {
        Ignoring i;
        i.play(
            tester({single(rarity, IgnoreRule::Mode::Exclude), single(life), single(armour), single(life)}));
        // the item's own first, and each once
        QCOMPARE(i.keys(), QStringList({armour, life, rarity}));
        QCOMPARE(i.ignored.index(0).data(Qt::DisplayRole).toString(), u"Armour"_s);
        QCOMPARE(i.of(armour, IgnoredStats::NameRole).toString(), u"Armour"_s);
        QCOMPARE(i.of(armour, IgnoredStats::NoteRole).toString(), u"The item's own, with its mods"_s);
        QCOMPARE(i.of(life, IgnoredStats::NameRole).toString(), life);
        QCOMPARE(i.of(life, IgnoredStats::NoteRole).toString(), QString());
        QCOMPARE(i.of(life, IgnoredStats::ModeRole).toInt(), int(IgnoredStats::Ignored));
        QCOMPARE(i.of(rarity, IgnoredStats::ModeRole).toInt(), int(IgnoredStats::Excluded));
        QCOMPARE(i.ignored.keys(), QStringList({armour, life, rarity}));
    }

    void alreadyPlaying()
    {
        Ignoring i;
        i.play(tester({single(life), group(u"mana"_s)}));
        IgnoredStats later(&i.active);
        QCOMPARE(later.rowCount(), 1);
        QCOMPARE(later.groups().size(), groupIds.size());
    }

    void bothWays()
    {
        Ignoring i;
        i.play(tester({group(u"res"_s), single(fire, IgnoreRule::Mode::Exclude)}));
        QCOMPARE(i.keys(), QStringList({fire}));
        QCOMPARE(i.ignored.keys().count(fire), 1);
        QCOMPARE(i.ignored.keys().size(), 4);
    }

    void ignoreGroup()
    {
        Ignoring i;
        i.play(tester({single(life)}));
        QSignalSpy changed(&i.ignored, &IgnoredStats::changed);
        i.ignored.setGroupMode(u"attributes"_s, IgnoredStats::Ignored);
        QCOMPARE(i.saved(), QList<IgnoreRule>({single(life), group(u"attributes"_s)}));
        QCOMPARE(i.groupMode(u"attributes"_s), int(IgnoredStats::Ignored));
        QCOMPARE(changed.size(), 1);

        i.ignored.setGroupMode(u"attributes"_s, IgnoredStats::Excluded);
        QCOMPARE(i.saved(),
                 QList<IgnoreRule>({single(life), group(u"attributes"_s, IgnoreRule::Mode::Exclude)}));
        i.ignored.setGroupMode(u"attributes"_s, IgnoredStats::Counts);
        QCOMPARE(i.saved(), QList<IgnoreRule>({single(life)}));
        QCOMPARE(i.groupMode(u"attributes"_s), int(IgnoredStats::Counts));
        QCOMPARE(changed.size(), 3);

        // or straight to excluded
        i.ignored.setGroupMode(u"life"_s, IgnoredStats::Excluded);
        QCOMPARE(i.saved(), QList<IgnoreRule>({single(life), group(u"life"_s, IgnoreRule::Mode::Exclude)}));
        QCOMPARE(i.groupMode(u"life"_s), int(IgnoredStats::Excluded));
    }

    void groupInPlace()
    {
        Ignoring i;
        i.play(tester({group(u"mana"_s), single(life)}));
        i.ignored.setGroupMode(u"mana"_s, IgnoredStats::Excluded);
        QCOMPARE(i.saved(), QList<IgnoreRule>({group(u"mana"_s, IgnoreRule::Mode::Exclude), single(life)}));
    }

    void wrongGroupModes()
    {
        Ignoring i;
        i.play(tester({group(u"mana"_s)}));
        QSignalSpy changed(&i.ignored, &IgnoredStats::changed);
        i.ignored.setGroupMode(u"mana"_s, IgnoredStats::Ignored);
        i.ignored.setGroupMode(u"mana"_s, 3);
        i.ignored.setGroupMode(u"mana"_s, -1);
        i.ignored.setGroupMode(u"luck"_s, IgnoredStats::Ignored);
        i.ignored.setGroupMode(u"life"_s, IgnoredStats::Counts);
        QCOMPARE(changed.size(), 0);
        QCOMPARE(i.saved(), QList<IgnoreRule>({group(u"mana"_s)}));
    }

    void addAndRemove()
    {
        Ignoring i;
        i.play(tester({group(u"res"_s)}));
        QSignalSpy reset(&i.ignored, &IgnoredStats::modelReset);
        i.ignored.add(rarity);
        QCOMPARE(i.saved(), QList<IgnoreRule>({group(u"res"_s), single(rarity)}));
        QCOMPARE(i.keys(), QStringList({rarity}));
        QCOMPARE(reset.size(), 1);
        i.ignored.add(rarity);
        i.ignored.add(QString());
        QCOMPARE(i.saved().size(), 2);

        // though a group has it
        i.ignored.add(fire);
        QCOMPARE(i.keys(), QStringList({rarity, fire}));

        i.ignored.remove(rarity);
        QCOMPARE(i.saved(), QList<IgnoreRule>({group(u"res"_s), single(fire)}));
        QCOMPARE(i.keys(), QStringList({fire}));
        QSignalSpy changed(&i.ignored, &IgnoredStats::changed);
        i.ignored.remove(life);
        i.ignored.remove(QString());
        QCOMPARE(changed.size(), 0);
        QCOMPARE(i.saved().size(), 2);
    }

    void removeEvery()
    {
        Ignoring i;
        i.play(tester({single(life), single(rarity), single(life)}));
        i.ignored.remove(life);
        QCOMPARE(i.saved(), QList<IgnoreRule>({single(rarity)}));
    }

    void setMode()
    {
        Ignoring i;
        i.play(tester({single(life), single(rarity)}));
        QSignalSpy reset(&i.ignored, &IgnoredStats::modelReset);
        QSignalSpy dataChanged(&i.ignored, &IgnoredStats::dataChanged);
        i.ignored.setMode(life, IgnoredStats::Excluded);
        QCOMPARE(i.saved(), QList<IgnoreRule>({single(life, IgnoreRule::Mode::Exclude), single(rarity)}));
        QCOMPARE(i.of(life, IgnoredStats::ModeRole).toInt(), int(IgnoredStats::Excluded));
        QCOMPARE(reset.size(), 0);
        QCOMPARE(dataChanged.size(), 1);
        QCOMPARE(dataChanged.at(0).at(2).value<QList<int>>(), QList<int>({IgnoredStats::ModeRole}));

        i.ignored.setMode(life, IgnoredStats::Ignored);
        QCOMPARE(i.saved(), QList<IgnoreRule>({single(life), single(rarity)}));
    }

    void wrongModes()
    {
        Ignoring i;
        i.play(tester({single(life), group(u"res"_s)}));
        QSignalSpy changed(&i.ignored, &IgnoredStats::changed);
        i.ignored.setMode(life, IgnoredStats::Ignored);
        i.ignored.setMode(life, IgnoredStats::Counts);
        i.ignored.setMode(life, 3);
        // only by itself
        i.ignored.setMode(fire, IgnoredStats::Excluded);
        // which a group has no key for
        i.ignored.setMode(QString(), IgnoredStats::Excluded);
        QCOMPARE(changed.size(), 0);
        QCOMPARE(i.saved(), QList<IgnoreRule>({single(life), group(u"res"_s)}));
    }

    void changedElsewhere()
    {
        Ignoring i;
        const QString id = i.play(tester({single(life)}));
        Character c = *i.store.find(id);
        c.ignored = {single(life, IgnoreRule::Mode::Exclude), group(u"life"_s)};
        QVERIFY(i.store.save(id, c));
        QSignalSpy dataChanged(&i.ignored, &IgnoredStats::dataChanged);
        i.active.reload();
        QCOMPARE(dataChanged.size(), 1);
        QCOMPARE(i.of(life, IgnoredStats::ModeRole).toInt(), int(IgnoredStats::Excluded));
        QCOMPARE(i.groupMode(u"life"_s), int(IgnoredStats::Ignored));
    }

    void cantSave()
    {
        Ignoring i;
        const QString id = i.play(tester());
        // where a folder is in the way
        const QString file = i.dir.filePath(u"characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QSignalSpy errorChanged(&i.ignored, &IgnoredStats::errorChanged);
        i.ignored.add(life);
        QVERIFY(i.ignored.error().startsWith(u"Couldn't save Tester: "_s));
        QCOMPARE(i.keys(), QStringList());
        i.ignored.add(life);
        QCOMPARE(errorChanged.size(), 1);

        QVERIFY(QDir().rmdir(file));
        i.ignored.setGroupMode(u"res"_s, IgnoredStats::Ignored);
        QCOMPARE(i.ignored.error(), QString());
        QCOMPARE(errorChanged.size(), 2);
    }

    void anotherCharacter()
    {
        Ignoring i;
        i.play(tester({single(life)}));
        Character other = tester({single(rarity)});
        other.name = u"Ottoline"_s;
        i.play(other);
        QCOMPARE(i.ignored.character(), u"Ottoline"_s);
        QCOMPARE(i.keys(), QStringList({rarity}));

        // and nobody
        i.active.setId(u"gone"_s);
        QCOMPARE(i.ignored.rowCount(), 0);
        QVERIFY(i.ignored.groups().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestIgnoredStats)
#include "tst_ignoredstats.moc"
