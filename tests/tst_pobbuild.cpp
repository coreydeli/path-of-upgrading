// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/pobbuild.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

const QString buildXml = u"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                         u"<PathOfBuilding2>\n"
                         u"\t<Build level=\"92\" className=\"Sorceress\" ascendClassName=\"Stormweaver\"/>\n"
                         u"</PathOfBuilding2>"_s;

// The zlib stream Path of Building puts in its codes. qCompress() writes the
// same with the length in front.
QByteArray zlibData(const QString &text)
{
    return qCompress(text.toUtf8()).sliced(4);
}

QString codeFor(const QByteArray &data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString decoded(const QString &code)
{
    return decodeBuildCode(code).value_or(u"(not a code)"_s);
}

const QString separator = u"--------"_s;

QString joined(const QStringList &lines)
{
    return lines.join(u'\n');
}

} // namespace

class TestPobBuild : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void decodesCodes() { QCOMPARE(decoded(codeFor(zlibData(buildXml))), buildXml); }

    void wrappedOverLines()
    {
        // and in the standard base64 alphabet, with padding
        QString code = QString::fromLatin1(zlibData(buildXml).toBase64());
        for (qsizetype i = 40; i < code.size(); i += 41)
            code.insert(i, u'\n');
        QCOMPARE(decoded(u"  "_s + code + u"\r\n"_s), buildXml);
    }

    void rawDeflate()
    {
        // without zlib's two byte header and the checksum at the end
        QCOMPARE(decoded(codeFor(zlibData(buildXml).sliced(2).chopped(4))), buildXml);
    }

    void plainXml() { QCOMPARE(decoded(u"\n  "_s + buildXml + u"\n"_s), buildXml); }

    void notACode()
    {
        QVERIFY(!decodeBuildCode(u"not a code!!"_s));
        QVERIFY(!decodeBuildCode(u"   "_s));
        // base64, but not deflated
        QVERIFY(!decodeBuildCode(codeFor("Path of Upgrading")));
        // cut short
        const QString code = codeFor(zlibData(buildXml));
        QVERIFY(!decodeBuildCode(code.first(code.size() / 2)));
    }

    void rareItem()
    {
        // as Path of Building saves it, the first line indented like the XML
        const QString raw = joined({
            u"\t\t\tRarity: RARE"_s,
            u"Doom Vise"_s,
            u"Sorcerer Gloves"_s,
            u"Energy Shield: 98"_s,
            u"Unique ID: 0a1b2c3d"_s,
            u"Item Level: 81"_s,
            u"Quality: 20"_s,
            u"Sockets: S S"_s,
            u"Rune: Glacial Rune"_s,
            u"Rune: None"_s,
            u"LevelReq: 65"_s,
            u"Implicits: 1"_s,
            u"{rune}+12% to Cold Resistance"_s,
            u"{tags:resistance}+39% to Lightning Resistance"_s,
            u"{crafted}{range:0.5}+(30-36) to maximum Life"_s,
            u"75% increased Energy Shield"_s,
            u"\t\t\t"_s,
        });
        const PobItemText item = convertPobItem(raw, u"Gloves"_s);
        QCOMPARE(item.text, joined({
                                u"Item Class: Gloves"_s,
                                u"Rarity: Rare"_s,
                                u"Doom Vise"_s,
                                u"Sorcerer Gloves"_s,
                                separator,
                                u"Energy Shield: 98"_s,
                                separator,
                                u"Requires: Level 65"_s,
                                separator,
                                u"Item Level: 81"_s,
                                separator,
                                // the rune is what the item's one implicit line is
                                u"+12% to Cold Resistance (rune)"_s,
                                separator,
                                u"+39% to Lightning Resistance"_s,
                                u"+33 to maximum Life (crafted)"_s,
                                u"75% increased Energy Shield"_s,
                            }));
        QVERIFY(item.hasBase);
        QVERIFY(parseItem(item.text));
    }

    void rollsAndTags()
    {
        const QString raw = joined({
            u"Rarity: RARE"_s,
            u"Cataclysm Clasp"_s,
            u"Jade Amulet"_s,
            u"Item Level: 79"_s,
            u"LevelReq: 60"_s,
            u"Implicits: 2"_s,
            u"{enchant}{range:1}(5-10)% increased Rarity of Items found"_s,
            u"+(10-15) to Dexterity"_s,
            u"{fractured}{range:0}+(80-89) to maximum Life"_s,
            u"{range:0.5}Adds (5-8) to (12-15) Fire Damage to Attacks"_s,
            u"{range:0.3}(0.2-0.4)% of Physical Attack Damage Leeched as Life"_s,
            u"{range:0}(-10-10)% increased Mana Cost of Skills"_s,
            u"{range:0}-(20-10)% to Chaos Resistance"_s,
            u"{desecrated}+(20-25)% to Cold Resistance"_s,
            u"Corrupted"_s,
        });
        const PobItemText item = convertPobItem(raw, u"Amulet"_s);
        QCOMPARE(item.text, joined({
                                u"Item Class: Amulets"_s,
                                u"Rarity: Rare"_s,
                                u"Cataclysm Clasp"_s,
                                u"Jade Amulet"_s,
                                separator,
                                u"Requires: Level 60"_s,
                                separator,
                                u"Item Level: 79"_s,
                                separator,
                                u"10% increased Rarity of Items found (enchant)"_s,
                                separator,
                                // halfway between, rounded up
                                u"+13 to Dexterity (implicit)"_s,
                                separator,
                                u"+80 to maximum Life (fractured)"_s,
                                u"Adds 7 to 14 Fire Damage to Attacks"_s,
                                u"0.26% of Physical Attack Damage Leeched as Life"_s,
                                u"10% reduced Mana Cost of Skills"_s,
                                u"-20% to Chaos Resistance"_s,
                                u"+23% to Cold Resistance (desecrated)"_s,
                                separator,
                                u"Corrupted"_s,
                            }));
        QVERIFY(!item.hasBase);
    }

    void variants()
    {
        const QString raw = joined({
            u"Rarity: UNIQUE"_s,
            u"Andvarius"_s,
            u"Gold Ring"_s,
            u"Variant: Pre 0.2.0"_s,
            u"Variant: Current"_s,
            u"LevelReq: 8"_s,
            u"Implicits: 2"_s,
            u"{variant:1}(6-15)% increased Rarity of Items found"_s,
            u"{variant:2}(10-20)% increased Rarity of Items found"_s,
            u"{range:1}(80-100)% increased Rarity of Items found"_s,
            u"{variant:1}-(10-20)% to Fire Resistance"_s,
            u"{variant:1,2}+(10-20)% to Cold Resistance"_s,
        });
        // no variant chosen is the current one, the last
        QCOMPARE(convertPobItem(raw, u"Ring 2"_s).text,
                 joined({
                     u"Item Class: Rings"_s,
                     u"Rarity: Unique"_s,
                     u"Andvarius"_s,
                     u"Gold Ring"_s,
                     separator,
                     u"Requires: Level 8"_s,
                     separator,
                     u"15% increased Rarity of Items found (implicit)"_s,
                     separator,
                     u"100% increased Rarity of Items found"_s,
                     u"+15% to Cold Resistance"_s,
                 }));

        const QString older = raw + u"\nSelected Variant: 1"_s;
        QVERIFY(convertPobItem(older, u"Ring 2"_s).text.contains(u"-15% to Fire Resistance"_s));
    }

    void duplicateLegacies()
    {
        const QString raw = joined({
            u"Rarity: UNIQUE"_s,
            u"Mageblood"_s,
            u"Utility Belt"_s,
            u"Variant: Legacy of Amethyst"_s,
            u"Variant: Legacy of Basalt"_s,
            u"Variant: Legacy of Bismuth"_s,
            u"Variant: Legacy of Diamond"_s,
            u"Selected Variant: 3"_s,
            u"Has Alt Variant: true"_s,
            u"Selected Alt Variant: 3"_s,
            u"Has Alt Variant Two: true"_s,
            u"Selected Alt Variant Two: 1"_s,
            // not on: there's no "Has Alt Variant Three"
            u"Selected Alt Variant Three: 2"_s,
            u"Allow Duplicate Variants: true"_s,
            u"Implicits: 2"_s,
            u"Has (1-3) Charm Slot"_s,
            u"20% of Flask Recovery applied Instantly"_s,
            u"{variant:1}Legacy of Amethyst"_s,
            u"{variant:2}Legacy of Basalt"_s,
            u"{variant:3}Legacy of Bismuth"_s,
            u"{variant:4}Legacy of Diamond"_s,
            u"{range:1}All Mage's Legacies have (25-50)% increased effect per duplicate Mage's Legacy you have"_s,
        });
        const auto item = parseItem(convertPobItem(raw, u"Belt"_s).text);
        QVERIFY(item);
        QVERIFY(item->legacies);
        QCOMPARE(item->legacies->names, (QStringList{u"Amethyst"_s, u"Bismuth"_s}));
        QCOMPARE(item->legacies->duplicates, 1);
        QCOMPARE(item->legacies->effect, 1.5);
    }

    void magicItem()
    {
        const QString raw = joined({
            u"Rarity: MAGIC"_s,
            u"Hale Omen Sceptre of the Whelpling"_s,
            u"Spirit: 100"_s,
            u"Item Level: 40"_s,
            u"Implicits: 0"_s,
            u"{range:0.5}+(25-35) to maximum Life"_s,
        });
        const PobItemText item = convertPobItem(raw, u"Weapon"_s);
        QCOMPARE(item.text, joined({
                                u"Item Class: Sceptres"_s,
                                u"Rarity: Magic"_s,
                                u"Hale Omen Sceptre of the Whelpling"_s,
                                separator,
                                u"Spirit: 100"_s,
                                separator,
                                u"Item Level: 40"_s,
                                separator,
                                u"+30 to maximum Life"_s,
                            }));
        QVERIFY(item.hasBase);
    }

    void itemClasses_data()
    {
        QTest::addColumn<QString>("slot");
        QTest::addColumn<QString>("baseType");
        QTest::addColumn<QString>("itemClass");

        const auto row = [](const QString &slot, const QString &baseType, const QString &itemClass) {
            QTest::addRow("%s", qPrintable(baseType)) << slot << baseType << itemClass;
        };
        row(u"Ring 2"_s, u"Gold Ring"_s, u"Rings"_s);
        row(u"Jewel 3"_s, u"Ruby"_s, u"Jewels"_s);
        row(u"Charm 2"_s, u"Thawing Charm"_s, u"Charms"_s);
        row(u"Mana Flask"_s, u"Ultimate Mana Flask"_s, u"Mana Flasks"_s);
        row(u"Body Armour"_s, u"Vile Robe"_s, u"Body Armours"_s);
        row(u"Weapon"_s, u"Omen Sceptre"_s, u"Sceptres"_s);
        row(u"Weapon"_s, u"Sword Spear"_s, u"Spears"_s);
        row(u"Weapon"_s, u"Wrapped Quarterstaff"_s, u"Quarterstaves"_s);
        row(u"Weapon"_s, u"Ashen Staff"_s, u"Staves"_s);
        row(u"Weapon"_s, u"Trarthan Cannon"_s, u"Crossbows"_s);
        row(u"Weapon 2"_s, u"Heavy Bow"_s, u"Bows"_s);
        row(u"Weapon"_s, u"Splitting Greataxe"_s, u"Two Hand Axes"_s);
        row(u"Weapon"_s, u"Battle Axe"_s, u"One Hand Axes"_s);
        row(u"Weapon"_s, u"Iron Greatsword"_s, u"Two Hand Swords"_s);
        row(u"Weapon"_s, u"Energy Blade Two Handed"_s, u"Two Hand Swords"_s);
        row(u"Weapon"_s, u"Energy Blade One Handed"_s, u"One Hand Swords"_s);
        row(u"Weapon"_s, u"Ethereal Blade"_s, u"Daggers"_s);
        row(u"Weapon"_s, u"Glass Shank"_s, u"Daggers"_s);
        row(u"Weapon"_s, u"Forge Maul"_s, u"Two Hand Maces"_s);
        row(u"Weapon"_s, u"Morning Star"_s, u"One Hand Maces"_s);
        row(u"Weapon"_s, u"Runic Fork"_s, u"Wands"_s);
        row(u"Offhand"_s, u"Leather Buckler"_s, u"Bucklers"_s);
        row(u"Offhand"_s, u"Hardwood Targe"_s, u"Shields"_s);
        row(u"Offhand"_s, u"Twig Focus"_s, u"Foci"_s);
        row(u"Offhand"_s, u"Broadhead Quiver"_s, u"Quivers"_s);
        row(u"Offhand 2"_s, u"Golden Flame"_s, u"Off-hands"_s);
        row(u"Weapon"_s, u"Something New"_s, u"Weapons"_s);
    }

    void itemClasses()
    {
        QFETCH(QString, slot);
        QFETCH(QString, baseType);
        QFETCH(QString, itemClass);
        QCOMPARE(pobItemClass(slot, baseType), itemClass);
    }
};

QTEST_GUILESS_MAIN(TestPobBuild)
#include "tst_pobbuild.moc"
