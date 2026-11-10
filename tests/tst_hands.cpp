// SPDX-License-Identifier: MIT
#include "core/hands.h"
#include "core/parser.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

Item rare(const QString &itemClass, const QString &name, const QString &base, const QString &mods = {})
{
    return parseItem(u"Item Class: %1\nRarity: Rare\n%2\n%3\n--------\n%4"_s.arg(itemClass, name, base, mods))
        .value();
}

const Item axe = rare(u"Two Hand Axes"_s, u"Doom Edge"_s, u"Stone Greataxe"_s);
const Item maul = rare(u"Two Hand Maces"_s, u"Skull Knell"_s, u"Stone Maul"_s);
const Item greatsword = rare(u"Two Hand Swords"_s, u"Rust Fang"_s, u"Iron Greatsword"_s);
const Item staff = rare(u"Staves"_s, u"Doom Pillar"_s, u"Ashen Staff"_s);
const Item quarterstaff = rare(u"Quarterstaves"_s, u"Wind Rod"_s, u"Long Quarterstaff"_s);
const Item crossbow = rare(u"Crossbows"_s, u"Grim Latch"_s, u"Heavy Crossbow"_s);
const Item wand = rare(u"Wands"_s, u"Ash Spire"_s, u"Bone Wand"_s);
const Item focus = rare(u"Foci"_s, u"Glow Lens"_s, u"Twig Focus"_s);
const Item shield = rare(u"Shields"_s, u"Dusk Wall"_s, u"Oak Shield"_s);
const Item helmet = rare(u"Helmets"_s, u"Titan Brow"_s, u"Iron Circlet"_s,
                         u"You can wield Two-Handed Axes, Maces and Swords in one hand"_s);
const Item plainHelmet = rare(u"Helmets"_s, u"Dull Brow"_s, u"Iron Circlet"_s);

const Wielding giant{.twoHandedInOne = true};
const Wielding varashta{.focusWithStaff = true};

} // namespace

class TestHands : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void oneHandedWeaponsAlwaysDo()
    {
        QVERIFY(twoHandedWithOffhand(wand, &shield, {}, {}));
        QVERIFY(twoHandedWithOffhand(wand, nullptr, {}, {}));
    }

    void twoHandedWeaponsDontByDefault()
    {
        for (const Item *weapon : {&axe, &maul, &greatsword, &staff, &quarterstaff, &crossbow}) {
            QVERIFY(!twoHandedWithOffhand(*weapon, nullptr, {}, {}));
            QVERIFY(!twoHandedWithOffhand(*weapon, &shield, {}, {}));
        }
    }

    void giantsBloodIsForAxesMacesAndSwords()
    {
        for (const Item *weapon : {&axe, &maul, &greatsword})
            QVERIFY(twoHandedWithOffhand(*weapon, &shield, {}, giant));
        for (const Item *weapon : {&staff, &quarterstaff, &crossbow})
            QVERIFY(!twoHandedWithOffhand(*weapon, &shield, {}, giant));
    }

    void anItemCanGiveGiantsBlood()
    {
        const Equipped worn = {{u"Helmet"_s, helmet}};
        QVERIFY(twoHandedWithOffhand(axe, &shield, worn, {}));
        QVERIFY(!twoHandedWithOffhand(staff, &shield, worn, {}));
        QVERIFY(!twoHandedWithOffhand(axe, &shield, {{u"Helmet"_s, plainHelmet}}, {}));
    }

    void varashtaUsesAFocusWithAStaff()
    {
        QVERIFY(twoHandedWithOffhand(staff, &focus, {}, varashta));
        QVERIFY(twoHandedWithOffhand(staff, nullptr, {}, varashta));
        QVERIFY(!twoHandedWithOffhand(staff, &shield, {}, varashta));
        QVERIFY(!twoHandedWithOffhand(quarterstaff, &focus, {}, varashta));
        QVERIFY(!twoHandedWithOffhand(axe, &focus, {}, varashta));
    }

    void offhandOutOfUseBesideTwoHands()
    {
        const Equipped staffed = {{u"Weapon"_s, staff}};
        QVERIFY(offhandOutOfUse(staffed, {}, u"Offhand"_s));
        // the other weapon set has its own
        QVERIFY(!offhandOutOfUse(staffed, {}, u"Offhand 2"_s));
        QVERIFY(!offhandOutOfUse(staffed, varashta, u"Offhand"_s));
        QVERIFY(!offhandOutOfUse({{u"Weapon"_s, wand}}, {}, u"Offhand"_s));
        QVERIFY(!offhandOutOfUse({}, {}, u"Offhand"_s));
        QVERIFY(!offhandOutOfUse(staffed, {}, u"Weapon"_s));
        QVERIFY(!offhandOutOfUse(staffed, {}, u"Helmet"_s));
        QVERIFY(offhandOutOfUse({{u"Weapon 2"_s, axe}}, {}, u"Offhand 2"_s));
        QVERIFY(!offhandOutOfUse({{u"Weapon"_s, axe}}, giant, u"Offhand"_s));
    }
};

QTEST_GUILESS_MAIN(TestHands)
#include "tst_hands.moc"
