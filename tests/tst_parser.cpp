// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "itemtexts.h"

#include <QTest>

using namespace pou;

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

        const auto rune = std::find_if(it->lines.cbegin(), it->lines.cend(),
                                       [](const ItemLine &l) { return l.text.contains(u"Cold"); });
        QCOMPARE(rune->tag, u"rune"_s);
        QCOMPARE(rune->key, u"#% to Cold Resistance"_s);
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
