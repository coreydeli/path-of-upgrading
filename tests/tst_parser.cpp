// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "itemtexts.h"

#include <QTest>

using namespace pou;

namespace {

// The first line of the item whose text contains `part`.
ItemLine lineWith(const Item &item, QStringView part)
{
    for (const ItemLine &l : item.lines) {
        if (l.text.contains(part))
            return l;
    }
    return {};
}

} // namespace

class TestParser : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void headerSlotAndProperties()
    {
        const auto it = parseItem(itemtexts::helmetNew);
        QVERIFY(it);
        QCOMPARE(it->itemClass, u"Helmets"_s);
        QCOMPARE(it->rarity, u"Rare"_s);
        QCOMPARE(it->slot, u"Helmet"_s);
        QCOMPARE(it->name, u"Doom Visage"_s);
        QCOMPARE(it->baseType, u"Expert Visored Helm"_s);
        QCOMPARE(it->itemLevel, 79);
        QCOMPARE(it->requiredLevel, 65);
        QCOMPARE(it->stats.value(u"[Base] Armour"), 312.0);
        QCOMPARE(it->stats.value(u"# to maximum Life"), 89.0);
    }

    void expandsAllResAndSumsRunes()
    {
        const auto it = parseItem(itemtexts::helmetNew);
        QCOMPARE(it->stats.value(u"#% to Fire Resistance"), 43.0);
        QCOMPARE(it->stats.value(u"#% to Cold Resistance"), 23.0);
        QCOMPARE(it->stats.value(u"#% to Lightning Resistance"), 11.0);
        QVERIFY(!it->stats.contains(u"#% to all Elemental Resistances"));
        QCOMPARE(it->stats.value(u"#% increased Attribute Requirements"), -12.0);

        const ItemLine rune = lineWith(*it, u"Cold");
        QCOMPARE(rune.tag, u"rune"_s);
        QCOMPARE(rune.key, u"#% to Cold Resistance"_s);
        QCOMPARE(it->lines.first().tag, u"base"_s);
    }

    void windowsLineEndings()
    {
        const auto it = parseItem(itemtexts::helmetOld);
        QVERIFY(it);
        QCOMPARE(it->name, u"Grim Crown"_s);
        QCOMPARE(it->stats.value(u"# to maximum Life"), 55.0);
        QVERIFY(!it->raw.contains(u'\r'));
    }

    void advancedCopy()
    {
        const auto ring = parseItem(itemtexts::advancedRing);
        QCOMPARE(ring->slot, u"Ring"_s);
        QCOMPARE(ring->stats.value(u"# to maximum Life"), 62.0);
        QCOMPARE(ring->stats.value(u"#% to Cold Resistance"), 24.0);
        QCOMPARE(ring->stats.value(u"#% to Lightning Resistance"), 30.0);
        QCOMPARE(ring->stats.value(u"# to Strength"), 12.0);
        QCOMPARE(ring->stats.value(u"# to Dexterity"), 12.0);
        QCOMPARE(lineWith(*ring, u"Cold").tag, u"implicit"_s);
        QCOMPARE(lineWith(*ring, u"Life").tag, u"explicit"_s);
    }

    void advancedCopyQuirks()
    {
        const auto it =
            parseItem(u"Item Class: Boots\nRarity: Rare\nOblivion Trail\nSorcerer Boots\n--------\n"
                      "Energy Shield: 414 (augmented)\nRunic Ward: 217 (augmented)\n--------\n"
                      "+60(48-60) to maximum Energy Shield\n35% increased Movement Speed\n"
                      "60% increased effect of Socketed Augment Items — Unscalable Value\n"
                      "Fractured Item"_s);
        QCOMPARE(it->stats.value(u"#% increased effect of Socketed Augment Items"), 60.0);
        QCOMPARE(it->stats.value(u"[Base] Runic Ward"), 217.0);
        QCOMPARE(it->stats.value(u"# to maximum Energy Shield"), 60.0);
        for (const QString &key : it->stats.keys())
            QVERIFY2(!key.contains(u"Fractured"), qPrintable(key));
    }

    void openAffixes()
    {
        const QString gloves =
            u"Item Class: Gloves\nRarity: Rare\nDoom Vise\nSorcerer Gloves\n--------\n"
            "Energy Shield: 280\n--------\n"
            "{ Prefix Modifier \"Hale\" (Tier: 3) — Life }\n+33(33-41) to maximum Life\n"
            "{ Prefix Modifier \"Shining\" (Tier: 2) — Defences, Energy Shield }\n"
            "75(68-79)% increased Energy Shield\n+20(18-22) to maximum Energy Shield\n"
            "{ Suffix Modifier \"of the Squall\" (Tier: 3) — Elemental, Lightning, Resistance }\n"
            "+39(36-40)% to Lightning Resistance"_s;
        const auto it = parseItem(gloves);
        QVERIFY(it->affixes.known);
        QCOMPARE(it->affixes.prefixes, 2);
        QCOMPARE(it->affixes.suffixes, 1);
        QCOMPARE(it->affixes.openPrefix, 1);
        QCOMPARE(it->affixes.openSuffix, 2);
        QCOMPARE(it->stats.value(u"# to maximum Energy Shield"), 20.0); // the hybrid mod's second line

        const auto corrupted = parseItem(gloves + u"\n--------\nCorrupted"_s);
        QVERIFY(corrupted->corrupted);
        QCOMPARE(corrupted->affixes.openPrefix, 0);
        QCOMPARE(corrupted->affixes.openSuffix, 0);

        QVERIFY(!parseItem(itemtexts::helmetOld)->affixes.known);
    }

    void implicitHeader()
    {
        const auto it =
            parseItem(u"Item Class: Rings\nRarity: Rare\nEmpyrean Gyre\nIron Ring\n--------\n"
                      "Item Level: 81\n--------\n{ Implicit Modifier }\n"
                      "Adds 1 to 4 Physical Damage to Attacks\n--------\n"
                      "{ Prefix Modifier \"Sapphire\" (Tier: 2) — Mana }\n+56(55-64) to maximum Mana\n"
                      "{ Suffix Modifier \"of the Sage\" (Tier: 1) — Attribute }\n"
                      "+31(31-33) to Intelligence"_s);
        QCOMPARE(lineWith(*it, u"Physical").tag, u"implicit"_s);
        QCOMPARE(lineWith(*it, u"Mana").tag, u"explicit"_s);
    }

    void weaponDps()
    {
        const auto mace = parseItem(itemtexts::twoHandMace);
        QCOMPARE(mace->slot, u"Weapon"_s);
        QVERIFY(mace->twoHanded);
        QCOMPARE(mace->stats.value(u"[Weapon] Physical DPS"), 143.0);
        QCOMPARE(mace->stats.value(u"[Weapon] Elemental DPS"), 33.0);
        QCOMPARE(mace->stats.value(u"[Weapon] Total DPS"), 176.0);
        QCOMPARE(mace->stats.value(u"[Weapon] Critical Hit Chance"), 5.0);
        QCOMPARE(mace->stats.value(u"[Weapon] Attacks per Second"), 1.1);
        QVERIFY(!mace->stats.contains(u"[Weapon] Chaos DPS"));
        QCOMPARE(mace->stats.value(u"Adds # to # Fire Damage"), 30.0);

        const auto dagger = parseItem(u"Item Class: Daggers\nRarity: Magic\nVile Dagger\n--------\n"
                                      "Physical Damage: 10-20\nChaos Damage: 5-9 (augmented), 1-3\n"
                                      "Attacks per Second: 1.55"_s);
        QVERIFY(!dagger->twoHanded);
        QCOMPARE(dagger->stats.value(u"[Weapon] Chaos DPS"), 14.0); // 13.95 rounds up
        QCOMPARE(dagger->stats.value(u"[Weapon] Total DPS"), 37.2);
        QVERIFY(!dagger->stats.contains(u"[Weapon] Critical Hit Chance"));
    }

    void expandsPairedStats()
    {
        QCOMPARE(
            expandStat(u"#% to Fire and Chaos Resistances"_s, 10),
            QList<StatMap::Entry>({{u"#% to Fire Resistance"_s, 10}, {u"#% to Chaos Resistance"_s, 10}}));
        QCOMPARE(expandStat(u"# to all Attributes"_s, 5).size(), 3);
        QCOMPARE(expandStat(u"# to maximum Life"_s, 5), QList<StatMap::Entry>({{u"# to maximum Life"_s, 5}}));
    }

    void notItems()
    {
        QVERIFY(!parseItem(u"hello"_s));
        QVERIFY(!parseItem(QString()));

        const auto currency = parseItem(
            u"Item Class: Stackable Currency\nRarity: Currency\nDivine Orb\n--------\nStack Size: 1/20"_s);
        QVERIFY(currency);
        QVERIFY(currency->slot.isEmpty());

        const auto flask =
            parseItem(u"Item Class: Life Flasks\nRarity: Magic\nFlask\n--------\nItem Level: 10"_s);
        QCOMPARE(flask->slot, u"Life Flask"_s);
        QVERIFY(flask->baseType.isEmpty()); // magic: the one name line has affixes in it
    }

    void normalItemsLoseSuperior()
    {
        const auto it = parseItem(
            u"Item Class: Boots\nRarity: Normal\nSuperior Leather Boots\n--------\nEvasion Rating: 20"_s);
        QCOMPARE(it->baseType, u"Leather Boots"_s);
        QCOMPARE(it->stats.value(u"[Base] Evasion Rating"), 20.0);
    }

    void infoAndFlavourLines()
    {
        const auto it = parseItem(u"Item Class: Wands\nRarity: Rare\nA\nWithered Wand\n--------\n"
                                  "Grants Skill: Level 11 Chaos Bolt\n--------\n"
                                  "Gain 10% of Damage as Extra Chaos Damage\nCan't be used with Shields"_s);
        QCOMPARE(it->lines.size(), 3);
        QCOMPARE(it->lines.at(0).tag, u"info"_s);
        QVERIFY(it->lines.at(0).key.isEmpty());
        QCOMPARE(it->lines.at(2).text, u"Can't be used with Shields"_s);
        QVERIFY(it->lines.at(2).key.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestParser)
#include "tst_parser.moc"
