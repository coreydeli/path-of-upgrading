// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/wearing.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

Item rare(const QString &itemClass, const QString &name, const QString &base, const QString &mods = {})
{
    return parseItem(u"Item Class: %1\nRarity: Rare\n%2\n%3\n--------\n%4"_s.arg(itemClass, name, base, mods))
        .value();
}

const Item helmet = rare(u"Helmets"_s, u"Storm Crown"_s, u"Twig Circlet"_s, u"+20% to Fire Resistance"_s);
const Item ring = rare(u"Rings"_s, u"Dusk Loop"_s, u"Iron Ring"_s);
const Item wand = rare(u"Wands"_s, u"Ash Spire"_s, u"Bone Wand"_s);
const Item staff = rare(u"Staves"_s, u"Doom Pillar"_s, u"Ashen Staff"_s);
const Item focus = rare(u"Foci"_s, u"Glow Lens"_s, u"Twig Focus"_s);
const Item quiver = rare(u"Quivers"_s, u"Wind Sheath"_s, u"Fire Quiver"_s);
const Item bow = rare(u"Bows"_s, u"Grim Arc"_s, u"Crude Bow"_s);

Character wearing(std::initializer_list<std::pair<QString, Item>> gear)
{
    Character c{.name = u"Tester"_s};
    for (const auto &[slot, item] : gear)
        c.gear.insert(slot, item);
    return c;
}

} // namespace

class TestWearing : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void whereItGoes()
    {
        QCOMPARE(slotsFor(helmet), QStringList{u"Helmet"_s});
        QCOMPARE(slotsFor(ring), (QStringList{u"Ring 1"_s, u"Ring 2"_s}));
        QCOMPARE(slotsFor(wand), (QStringList{u"Weapon"_s, u"Weapon 2"_s}));
        QCOMPARE(slotsFor(focus), (QStringList{u"Offhand"_s, u"Offhand 2"_s}));
        QCOMPARE(slotsFor(rare(u"Jewels"_s, u"Pale Eye"_s, u"Ruby"_s)).size(), 6);

        Item currency;
        currency.itemClass = u"Stackable Currency"_s;
        QVERIFY(slotsFor(currency).isEmpty());
    }

    void putsItOn()
    {
        const std::optional<Character> c = wear(wearing({}), u"Ring 2"_s, ring);
        QVERIFY(c);
        QCOMPARE(c->name, u"Tester"_s);
        QCOMPARE(c->gear.keys(), QStringList{u"Ring 2"_s});
        QCOMPARE(c->gear.value(u"Ring 2"_s).name, u"Dusk Loop"_s);
    }

    void inPlaceOfTheBuilds()
    {
        Character c = wearing({{u"Helmet"_s, helmet}, {u"Ring 1"_s, ring}});
        c.fromPob = {u"Helmet"_s, u"Ring 1"_s};
        const Item other = rare(u"Helmets"_s, u"Gale Visage"_s, u"Twig Circlet"_s);

        const std::optional<Character> worn = wear(c, u"Helmet"_s, other);
        QVERIFY(worn);
        QCOMPARE(worn->gear.value(u"Helmet"_s).name, u"Gale Visage"_s);
        QCOMPARE(worn->fromPob, QSet<QString>{u"Ring 1"_s});
    }

    void notThere()
    {
        QVERIFY(!wear(wearing({}), u"Gloves"_s, helmet));
        QVERIFY(!wear(wearing({}), u"Offhand"_s, wand));
        QVERIFY(!wear(wearing({}), u"Neck"_s, helmet));

        Item currency;
        currency.itemClass = u"Stackable Currency"_s;
        QVERIFY(!wear(wearing({}), QString(), currency));
    }

    void twoHandsTakeTheOffhand()
    {
        const Character c = wearing({{u"Weapon"_s, wand}, {u"Offhand"_s, focus}, {u"Offhand 2"_s, focus}});
        const std::optional<Character> worn = wear(c, u"Weapon"_s, staff);
        QVERIFY(worn);
        QCOMPARE(worn->gear.value(u"Weapon"_s).name, u"Doom Pillar"_s);
        QVERIFY(!worn->gear.contains(u"Offhand"_s));
        // the other weapon set is another matter
        QVERIFY(worn->gear.contains(u"Offhand 2"_s));
    }

    void offhandTakesTwoHands()
    {
        Character c = wearing({{u"Weapon 2"_s, staff}, {u"Weapon"_s, staff}});
        c.fromPob = {u"Weapon 2"_s};
        const std::optional<Character> worn = wear(c, u"Offhand 2"_s, focus);
        QVERIFY(worn);
        QVERIFY(!worn->gear.contains(u"Weapon 2"_s));
        QVERIFY(worn->fromPob.isEmpty());
        QVERIFY(worn->gear.contains(u"Weapon"_s));
        QVERIFY(worn->gear.contains(u"Offhand 2"_s));
    }

    void oneHandKeepsTheOther()
    {
        const Character c = wearing({{u"Weapon"_s, wand}, {u"Offhand"_s, focus}});
        QCOMPARE(wear(c, u"Weapon"_s, wand)->gear.size(), 2);
        QCOMPARE(wear(c, u"Offhand"_s, focus)->gear.size(), 2);
        // a bow is held in both hands, but with a quiver
        QCOMPARE(wear(wearing({{u"Offhand"_s, quiver}}), u"Weapon"_s, bow)->gear.size(), 2);
        QCOMPARE(wear(wearing({{u"Weapon"_s, bow}}), u"Offhand"_s, quiver)->gear.size(), 2);
    }

    void resistancesFollowTheSameItem()
    {
        Character c = wearing({{u"Helmet"_s, helmet}});
        ResistEntry entry;
        entry.values[Element::Fire] = 60;
        c.resists = snapshotResists(entry, c.gear, ResistSource::Pob);

        // the game says more about it than the build did
        const Item copied = rare(u"Helmets"_s, u"Storm Crown"_s, u"Twig Circlet"_s,
                                 u"+20% to Fire Resistance\n+10% to Fire Resistance"_s);
        const std::optional<Character> worn = wear(c, u"Helmet"_s, copied);
        QCOMPARE(liveResists(*worn->resists, worn->gear)->values[Element::Fire].value(), 60.0);

        // while another item changes them
        const Item other = rare(u"Helmets"_s, u"Gale Visage"_s, u"Twig Circlet"_s);
        const std::optional<Character> swapped = wear(c, u"Helmet"_s, other);
        QCOMPARE(liveResists(*swapped->resists, swapped->gear)->values[Element::Fire].value(), 40.0);
    }

    void takesItOff()
    {
        Character c = wearing({{u"Helmet"_s, helmet}, {u"Ring 1"_s, ring}});
        c.fromPob = {u"Helmet"_s, u"Ring 1"_s};

        const Character off = takeOff(c, u"Helmet"_s);
        QCOMPARE(off.gear.keys(), QStringList{u"Ring 1"_s});
        QCOMPARE(off.fromPob, QSet<QString>{u"Ring 1"_s});
        QCOMPARE(takeOff(c, u"Gloves"_s).gear.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestWearing)
#include "tst_wearing.moc"
