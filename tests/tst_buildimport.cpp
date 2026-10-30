// SPDX-License-Identifier: MIT
#include "core/buildimport.h"
#include "core/parser.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

QString rare(const QString &itemClass, const QString &name, const QString &base, const QString &mods)
{
    return u"Item Class: %1\nRarity: Rare\n%2\n%3\n--------\n%4"_s.arg(itemClass, name, base, mods);
}

const QString helmet = rare(u"Helmets"_s, u"Storm Crown"_s, u"Twig Circlet"_s, u"+20% to Fire Resistance"_s);
const QString ring = rare(u"Rings"_s, u"Dusk Loop"_s, u"Iron Ring"_s, u"+30% to Cold Resistance"_s);
const QString wand = rare(u"Wands"_s, u"Ash Spire"_s, u"Bone Wand"_s, u"+1 to Level of all Spell Skills"_s);
const QString bow = rare(u"Bows"_s, u"Grim Arc"_s, u"Crude Bow"_s, u"+10 to Dexterity"_s);
const QString gloves =
    rare(u"Gloves"_s, u"Rot Grip"_s, u"Suede Bracers"_s, u"+15% to Lightning Resistance"_s);
const QString boots = rare(u"Boots"_s, u"Mire Tread"_s, u"Rough Greaves"_s, u"+10% to Fire Resistance"_s);
const QString belt = rare(u"Belts"_s, u"Bramble Cord"_s, u"Rawhide Belt"_s, u"+40 to maximum Life"_s);

Item item(const QString &text)
{
    return parseItem(text).value();
}

PobBuild buildWith(std::initializer_list<std::pair<QString, PobItemText>> gear)
{
    PobBuild build;
    for (const auto &[slot, text] : gear)
        build.gear.insert(slot, text);
    return build;
}

} // namespace

class TestBuildImport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void putsOnTheGear()
    {
        const PobBuild build = buildWith({
            {u"Helmet"_s, {.text = helmet}},
            {u"Ring 1"_s, {.text = ring}},
            {u"Weapon"_s, {.text = bow, .missingDamage = true}},
        });

        const BuildImport done = importBuild(Character{.name = u"Tester"_s}, build);
        QCOMPARE(done.items, 3);
        QCOMPARE(done.kept, 0);
        QCOMPARE(done.withoutDamage, 1);
        QCOMPARE(done.character.name, u"Tester"_s);
        QCOMPARE(done.character.fromPob, (QSet<QString>{u"Helmet"_s, u"Ring 1"_s, u"Weapon"_s}));
        QCOMPARE(done.character.gear.size(), 3);
        QCOMPARE(done.character.gear.value(u"Ring 1"_s).name, u"Dusk Loop"_s);
        QCOMPARE(done.character.gear.value(u"Weapon"_s).baseType, u"Crude Bow"_s);
    }

    void keepsWhatYouCopied()
    {
        Character character;
        // the same helmet copied in the game, which says more about it
        const QString copied = helmet + u"\n+5% to Chaos Resistance"_s;
        character.gear.insert(u"Helmet"_s, item(copied));
        // and gloves the build doesn't wear
        character.gear.insert(u"Gloves"_s, item(gloves));
        // nor a ring of the same name on another base
        character.gear.insert(u"Ring 1"_s,
                              item(rare(u"Rings"_s, u"Dusk Loop"_s, u"Gold Ring"_s, u"+9 to Strength"_s)));
        const PobBuild build = buildWith({
            {u"Helmet"_s, {.text = helmet}},
            {u"Ring 1"_s, {.text = ring}},
            {u"Gloves"_s,
             {.text = rare(u"Gloves"_s, u"Wraith Clasp"_s, u"Suede Bracers"_s, u"+9 to Strength"_s)}},
        });

        const BuildImport done = importBuild(character, build);
        QCOMPARE(done.items, 2);
        QCOMPARE(done.kept, 1);
        QCOMPARE(done.character.gear.value(u"Helmet"_s).raw, copied);
        QCOMPARE(done.character.gear.value(u"Gloves"_s).name, u"Wraith Clasp"_s);
        QCOMPARE(done.character.gear.value(u"Ring 1"_s).baseType, u"Iron Ring"_s);
        QCOMPARE(done.character.fromPob, (QSet<QString>{u"Gloves"_s, u"Ring 1"_s}));
    }

    void replacesTheBuildBefore()
    {
        Character character;
        character.gear.insert(u"Boots"_s, item(boots));
        character.gear.insert(u"Ring 1"_s, item(ring));
        character.fromPob = {u"Boots"_s, u"Ring 1"_s};
        // copied in the game, so it isn't the old build's
        character.gear.insert(u"Belt"_s, item(belt));

        const BuildImport done = importBuild(character, buildWith({{u"Ring 1"_s, {.text = ring}}}));
        QCOMPARE(done.character.gear.keys().size(), 2);
        QVERIFY(!done.character.gear.contains(u"Boots"_s));
        QCOMPARE(done.character.gear.value(u"Belt"_s).raw, belt);
        // the ring came with the old build, so it's the new build's now
        QCOMPARE(done.kept, 0);
        QCOMPARE(done.items, 1);
        QCOMPARE(done.character.fromPob, QSet<QString>{u"Ring 1"_s});
    }

    void jewels()
    {
        PobBuild build;
        for (const QString &name : {u"Glint"_s, u"Shard"_s})
            build.jewels << PobItemText{.text =
                                            rare(u"Jewels"_s, name, u"Ruby"_s, u"+8% to Fire Resistance"_s)};

        const BuildImport done = importBuild({}, build);
        QCOMPARE(done.items, 2);
        QCOMPARE(done.character.gear.value(u"Jewel 1"_s).name, u"Glint"_s);
        QCOMPARE(done.character.gear.value(u"Jewel 2"_s).name, u"Shard"_s);
        QCOMPARE(done.character.fromPob, (QSet<QString>{u"Jewel 1"_s, u"Jewel 2"_s}));
    }

    void moreJewelsThanSlots()
    {
        PobBuild build;
        for (int i = 0; i < 8; ++i)
            build.jewels << PobItemText{
                .text = rare(u"Jewels"_s, u"Gem %1"_s.arg(i), u"Emerald"_s, u"+5 to Dexterity"_s)};

        const BuildImport done = importBuild({}, build);
        QCOMPARE(done.items, 6);
        QCOMPARE(done.character.gear.value(u"Jewel 6"_s).name, u"Gem 5"_s);
    }

    void unreadableItems()
    {
        const PobBuild build = buildWith({
            {u"Weapon"_s, {.text = u"Rarity: Rare\nNot what the game copies"_s}},
            {u"Helmet"_s, {.text = helmet}},
        });

        const BuildImport done = importBuild({}, build);
        QCOMPARE(done.items, 1);
        QCOMPARE(done.character.fromPob, QSet<QString>{u"Helmet"_s});
    }

    void takesTheResists()
    {
        PobBuild build = buildWith({{u"Helmet"_s, {.text = helmet}}, {u"Ring 1"_s, {.text = ring}}});
        build.resists[Element::Fire] = 80;
        build.resists[Element::Cold] = 70;
        Character character;
        // what you wear where the build has nothing counts as a change from it
        character.gear.insert(u"Boots"_s, item(boots));
        character.resists = ResistRecord{.cap = 76, .source = ResistSource::Pob};

        const BuildImport done = importBuild(character, build);
        QVERIFY(done.resists);
        const ResistRecord &r = done.character.resists.value();
        QCOMPARE(r.source, ResistSource::Pob);
        QCOMPARE(r.entered[Element::Fire], 80);
        QCOMPARE(r.entered[Element::Cold], 70);
        QCOMPARE(r.entered[Element::Lightning], std::nullopt);
        QCOMPARE(r.cap, 76);
        // with the build's gear, not yours
        QCOMPARE(r.gear[Element::Fire], 20);
        QCOMPARE(r.gear[Element::Cold], 30);
        const auto live = liveResists(r, done.character.gear);
        QCOMPARE(live->values[Element::Fire], 90);
    }

    void keepsResistsFromTheSheet()
    {
        PobBuild build = buildWith({{u"Helmet"_s, {.text = helmet}}});
        build.resists[Element::Fire] = 80;
        Character character;
        character.resists = ResistRecord{.cap = 75, .source = ResistSource::Game};
        character.resists->entered[Element::Fire] = 60;

        const BuildImport done = importBuild(character, build);
        QVERIFY(!done.resists);
        QCOMPARE(done.character.resists, character.resists);
    }

    void buildWithoutResists()
    {
        Character character;
        character.resists = ResistRecord{.cap = 75, .source = ResistSource::Manual};
        character.resists->entered[Element::Cold] = 50;

        const BuildImport done = importBuild(character, buildWith({{u"Helmet"_s, {.text = helmet}}}));
        QVERIFY(!done.resists);
        QCOMPARE(done.character.resists, character.resists);
    }

    void theBuildsResists()
    {
        // without importing it
        PobBuild build = buildWith({{u"Helmet"_s, {.text = helmet}}});
        build.jewels << PobItemText{
            .text = rare(u"Jewels"_s, u"Glint"_s, u"Ruby"_s, u"+8% to Fire Resistance"_s)};
        build.resists[Element::Fire] = 80;
        ResistRecord before{.cap = 76, .buffer = 3, .bufferSet = true, .source = ResistSource::Game};
        before.entered[Element::Cold] = 50;

        const std::optional<ResistRecord> r = buildResists(build, &before);
        QVERIFY(r);
        QCOMPARE(r->source, ResistSource::Pob);
        QCOMPARE(r->entered[Element::Fire], 80);
        QCOMPARE(r->entered[Element::Cold], std::nullopt);
        QCOMPARE(r->cap, 76);
        QCOMPARE(r->buffer, 3);
        // jewels don't add to your resistances
        QCOMPARE(r->gear[Element::Fire], 20);

        QCOMPARE(buildResists(buildWith({{u"Helmet"_s, {.text = helmet}}})), std::nullopt);
    }
};

QTEST_GUILESS_MAIN(TestBuildImport)
#include "tst_buildimport.moc"
