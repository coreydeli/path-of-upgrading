// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/scorer.h"
#include "itemtexts.h"

#include <QTest>

using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

StatRow rowFor(const Comparison &r, QStringView key)
{
    for (const StatRow &row : r.rows) {
        if (row.key == key)
            return row;
    }
    return {};
}

} // namespace

class TestScorer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scoreIsWeightedSum()
    {
        QCOMPARE(score(item(itemtexts::helmetOld), {{u"# to maximum Life"_s, 2}}), 110.0);
        QCOMPARE(score(item(itemtexts::helmetOld), {}), 0.0);
    }

    void notGear()
    {
        const Comparison r = compare(
            item(
                u"Item Class: Stackable Currency\nRarity: Currency\nDivine Orb\n--------\nStack Size: 1/20"_s),
            {}, defaultWeights());
        QVERIFY(!r.ok);
        QVERIFY(r.reason.startsWith(u"Stackable Currency"));
    }

    void helmetUpgrade()
    {
        const Comparison r = compare(item(itemtexts::helmetNew), {{u"Helmet"_s, item(itemtexts::helmetOld)}},
                                     defaultWeights());
        QVERIFY(r.ok);
        QCOMPARE(r.verdict, Verdict::Upgrade);
        QVERIFY(r.delta > 0);
        QVERIFY(r.percent);
        QCOMPARE(r.replacing, QStringList{u"Grim Crown"_s});
        QCOMPARE(rowFor(r, u"# to maximum Life").diff, 34.0);

        const Comparison back = compare(item(itemtexts::helmetOld),
                                        {{u"Helmet"_s, item(itemtexts::helmetNew)}}, defaultWeights());
        QCOMPARE(back.verdict, Verdict::Downgrade);
    }

    void rowsSortByImpact()
    {
        const Comparison r = compare(item(itemtexts::helmetNew), {{u"Helmet"_s, item(itemtexts::helmetOld)}},
                                     defaultWeights());
        for (qsizetype i = 1; i < r.rows.size(); ++i)
            QVERIFY(std::abs(r.rows.at(i - 1).impact) >= std::abs(r.rows.at(i).impact));
        // stats only the old item had are listed too
        QCOMPARE(rowFor(r, u"#% to Lightning Resistance").oldValue, 18.0);
    }

    void ringReplacesWeakerOrFillsEmpty()
    {
        const Item ring = item(itemtexts::advancedRing);
        const Item weak =
            item(u"Item Class: Rings\nRarity: Magic\nWeak Ring\n--------\n+10 to maximum Life"_s);
        const Item strong =
            item(u"Item Class: Rings\nRarity: Rare\nStrong\nRing\n--------\n+200 to maximum Life"_s);

        const Comparison r = compare(ring, {{u"Ring 1"_s, strong}, {u"Ring 2"_s, weak}}, defaultWeights());
        QCOMPARE(r.targetSlot, u"Ring 2"_s);
        QCOMPARE(r.verdict, Verdict::Upgrade);

        const Comparison empty = compare(ring, {{u"Ring 1"_s, strong}}, defaultWeights());
        QCOMPARE(empty.targetSlot, u"Ring 2"_s);
        QCOMPARE(empty.verdict, Verdict::New);
        QVERIFY(empty.notes.last().contains(u"Ring 2"));
    }

    void slotYouName()
    {
        const Item ring = item(itemtexts::advancedRing);
        const Item weak =
            item(u"Item Class: Rings\nRarity: Magic\nWeak Ring\n--------\n+10 to maximum Life"_s);
        const Item strong =
            item(u"Item Class: Rings\nRarity: Rare\nStrong\nRing\n--------\n+200 to maximum Life"_s);
        const Equipped worn = {{u"Ring 1"_s, strong}, {u"Ring 2"_s, weak}};

        const Comparison r = compare(ring, worn, defaultWeights(), {}, u"Ring 1"_s);
        QCOMPARE(r.targetSlot, u"Ring 1"_s);
        QCOMPARE(r.replacing, QStringList{u"Strong"_s});
        QCOMPARE(rowFor(r, u"# to maximum Life").oldValue, 200.0);

        // an empty one is still empty
        const Comparison empty = compare(ring, {{u"Ring 1"_s, strong}}, defaultWeights(), {}, u"Ring 2"_s);
        QCOMPARE(empty.verdict, Verdict::New);
    }

    void emptyJewelSlotFirst()
    {
        const Item a = item(
            u"Item Class: Jewels\nRarity: Rare\nA\nSapphire\n--------\nMinions deal 10% increased Damage"_s);
        const Item b = item(
            u"Item Class: Jewels\nRarity: Rare\nB\nSapphire\n--------\nMinions deal 5% increased Damage"_s);
        QCOMPARE(compare(a, {{u"Jewel 1"_s, b}}, defaultWeights()).targetSlot, u"Jewel 2"_s);
    }

    void twoHanderAgainstWeaponAndOffhand()
    {
        const Item oneHand =
            item(u"Item Class: One Hand Maces\nRarity: Rare\nA\nB\n--------\n"
                 "Physical Damage: 10-20\nAttacks per Second: 1.50\n--------\n+30 to maximum Life"_s);
        const Item shield = item(
            u"Item Class: Shields\nRarity: Rare\nS\nT\n--------\nArmour: 200\n--------\n+50 to maximum Life"_s);
        const Comparison r = compare(item(itemtexts::twoHandMace),
                                     {{u"Weapon"_s, oneHand}, {u"Offhand"_s, shield}}, defaultWeights());
        QCOMPARE(r.replacing, QStringList({u"A"_s, u"S"_s}));
        QCOMPARE(rowFor(r, u"# to maximum Life").oldValue, 80.0);
        QVERIFY(
            r.notes.contains(u"Two-handed, so it's compared against your weapon and offhand together."_s));

        const Comparison offhand =
            compare(shield, {{u"Weapon"_s, item(itemtexts::twoHandMace)}}, defaultWeights());
        QVERIFY(offhand.notes.first().contains(u"two-handed"));

        // named, the weapon slot still means both
        const Comparison named =
            compare(item(itemtexts::twoHandMace), {{u"Weapon"_s, oneHand}, {u"Offhand"_s, shield}},
                    defaultWeights(), {}, u"Weapon"_s);
        QCOMPARE(named.replacing, QStringList({u"A"_s, u"S"_s}));
    }

    void weaponInTheOffhand()
    {
        // a one-handed weapon held in the other hand
        const Item mace =
            item(u"Item Class: One Hand Maces\nRarity: Rare\nA\nB\n--------\n"
                 "Physical Damage: 10-20\nAttacks per Second: 1.50\n--------\n+30 to maximum Life"_s);
        const Item other =
            item(u"Item Class: One Hand Maces\nRarity: Rare\nC\nD\n--------\n"
                 "Physical Damage: 10-20\nAttacks per Second: 1.50\n--------\n+60 to maximum Life"_s);
        const Comparison r =
            compare(mace, {{u"Weapon"_s, mace}, {u"Offhand"_s, other}}, defaultWeights(), {}, u"Offhand"_s);
        QCOMPARE(r.targetSlot, u"Offhand"_s);
        QCOMPARE(r.replacing, QStringList{u"C"_s});
        QVERIFY(r.notes.isEmpty());
    }

    void alreadyEquipped()
    {
        const Equipped worn = {{u"Helmet"_s, item(itemtexts::helmetOld)}};
        const Comparison same = compare(item(itemtexts::helmetOld), worn, defaultWeights());
        QVERIFY(same.isEquipped);
        QCOMPARE(same.verdict, Verdict::Equipped);
        QCOMPARE(same.delta, 0.0);
        QVERIFY(!compare(item(itemtexts::helmetNew), worn, defaultWeights()).isEquipped);
    }

    void equippedWhereYouWearIt()
    {
        // your other ring is weaker, but this one's in Ring 2
        const Item weak =
            item(u"Item Class: Rings\nRarity: Magic\nWeak Ring\n--------\n+10 to maximum Life"_s);
        const Item strong =
            item(u"Item Class: Rings\nRarity: Rare\nStrong\nRing\n--------\n+200 to maximum Life"_s);
        const Comparison ring =
            compare(strong, {{u"Ring 1"_s, weak}, {u"Ring 2"_s, strong}}, defaultWeights());
        QCOMPARE(ring.targetSlot, u"Ring 2"_s);
        QCOMPARE(ring.replacing, QStringList{u"Strong"_s});
        QCOMPARE(ring.delta, 0.0);
        QVERIFY(ring.rows.isEmpty());

        // in the other weapon set, by itself
        const Item mace = item(itemtexts::twoHandMace);
        const Item quiver =
            item(u"Item Class: Quivers\nRarity: Magic\nHale Quiver\n--------\n+20 to maximum Life"_s);
        const Comparison swap =
            compare(mace, {{u"Weapon 2"_s, mace}, {u"Offhand"_s, quiver}}, defaultWeights());
        QCOMPARE(swap.targetSlot, u"Weapon 2"_s);
        QCOMPARE(swap.replacing, QStringList{u"Brute Knell"_s});
        QCOMPARE(swap.verdict, Verdict::Equipped);
        QVERIFY(swap.notes.isEmpty());
    }

    void ignoredStatsAreLeftOut()
    {
        const auto ignoreLife = [](const QString &key) {
            return key == u"# to maximum Life";
        };
        const Comparison r = compare(item(itemtexts::helmetNew), {{u"Helmet"_s, item(itemtexts::helmetOld)}},
                                     defaultWeights(), ignoreLife);
        QVERIFY(rowFor(r, u"# to maximum Life").key.isEmpty());
        const Comparison all = compare(item(itemtexts::helmetNew),
                                       {{u"Helmet"_s, item(itemtexts::helmetOld)}}, defaultWeights());
        QCOMPARE(all.newScore - r.newScore, 89.0);
    }

    void unweightedChangesGetANote()
    {
        const Comparison r =
            compare(item(itemtexts::helmetNew), {{u"Helmet"_s, item(itemtexts::helmetOld)}}, {});
        QCOMPARE(r.verdict, Verdict::Same);
        QVERIFY(r.notes.last().contains(u"weight"));
    }
};

QTEST_GUILESS_MAIN(TestScorer)
#include "tst_scorer.moc"
