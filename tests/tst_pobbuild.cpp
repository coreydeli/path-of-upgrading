// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/pobbuild.h"
#include "pobfixtures.h"

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

QStringList sortedSlots(const PobBuild &build)
{
    QStringList keys = build.gear.keys();
    keys.sort();
    return keys;
}

QString nameOf(const PobItemText &item)
{
    const auto parsed = parseItem(item.text);
    return parsed ? parsed->name : u"(no item)"_s;
}

QString nameIn(const PobBuild &build, const QString &slot)
{
    return nameOf(build.gear.value(slot));
}

QStringList jewelNames(const PobBuild &build)
{
    QStringList out;
    for (const PobItemText &jewel : build.jewels)
        out << nameOf(jewel);
    return out;
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

    void shareLinks_data()
    {
        QTest::addColumn<QString>("link");
        QTest::addColumn<QString>("codeUrl");

        const auto row = [](const QString &link, const QString &codeUrl) {
            QTest::addRow("%s", qPrintable(link)) << link << codeUrl;
        };
        row(u"https://pobb.in/AbC_12-x"_s, u"https://pobb.in/pob/AbC_12-x"_s);
        row(u"https://pobb.in/u/someone/AbC"_s, u"https://pobb.in/pob/AbC"_s);
        row(u"  pobb.in/AbC/  "_s, u"https://pobb.in/pob/AbC"_s);
        row(u"https://www.pobb.in/AbC?utm_source=chat#notes"_s, u"https://pobb.in/pob/AbC"_s);
        row(u"https://pobb.in/AbC/raw"_s, u"https://pobb.in/pob/AbC"_s);
        row(u"https://poe.ninja/poe2/pob/1a2b"_s, u"https://poe.ninja/poe2/pob/raw/1a2b"_s);
        row(u"https://poe2.ninja/pob/1a2b"_s, u"https://poe.ninja/poe2/pob/raw/1a2b"_s);
        row(u"https://maxroll.gg/poe2/pob/xy12ab"_s, u"https://maxroll.gg/poe2/api/pob/xy12ab"_s);
        row(u"https://poe2db.tw/pob/AbCd"_s, u"https://poe2db.tw/pob/AbCd/raw"_s);
        row(u"https://pastebin.com/Ab12Cd34"_s, u"https://pastebin.com/raw/Ab12Cd34"_s);
        row(u"https://pastebin.com/raw/Ab12Cd34"_s, u"https://pastebin.com/raw/Ab12Cd34"_s);
        row(u"https://rentry.co/abc12"_s, u"https://rentry.co/paste/abc12/raw"_s);

        row(u"eNrtxyz"_s, QString());
        row(u"https://pobb.in/u/someone"_s, QString());
        row(u"https://poe.ninja/poe2/builds"_s, QString());
        row(u"https://example.com/AbC"_s, QString());
        row(u"ftp://pobb.in/AbC"_s, QString());
    }

    void shareLinks()
    {
        QFETCH(QString, link);
        QFETCH(QString, codeUrl);
        QCOMPARE(buildCodeUrl(link).toString(), codeUrl);
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
        QVERIFY(!item.missingDamage);
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
        QVERIFY(!item.missingDamage);
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
        // a sceptre has spirit rather than damage
        QVERIFY(!item.missingDamage);
    }

    void weaponDamage()
    {
        const auto missing = [](const QString &slot, const QString &baseType) {
            return convertPobItem(joined({u"Rarity: NORMAL"_s, baseType}), slot).missingDamage;
        };
        QVERIFY(missing(u"Weapon"_s, u"Wrapped Quarterstaff"_s));
        QVERIFY(missing(u"Weapon 2"_s, u"Heavy Bow"_s));
        QVERIFY(missing(u"Offhand"_s, u"Glass Shank"_s));
        // a base we don't know might attack
        QVERIFY(missing(u"Weapon"_s, u"Something New"_s));

        QVERIFY(!missing(u"Weapon"_s, u"Ashen Staff"_s));
        QVERIFY(!missing(u"Weapon"_s, u"Runic Fork"_s));
        QVERIFY(!missing(u"Offhand"_s, u"Broadhead Quiver"_s));
        QVERIFY(!missing(u"Offhand"_s, u"Leather Buckler"_s));
        QVERIFY(!missing(u"Gloves"_s, u"Sorcerer Gloves"_s));
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

    void readsABuild()
    {
        const auto build = parsePobBuild(decoded(codeFor(zlibData(pobfixtures::build))));
        QVERIFY(build);
        QCOMPARE(build->level, 92);
        QCOMPARE(build->className, u"Sorceress"_s);
        QCOMPARE(build->ascendancy, u"Stormweaver"_s);
        // what's shown, plus how far over the maximum
        QCOMPARE(build->resists[Element::Fire].value(), 93.0);
        QCOMPARE(build->resists[Element::Cold].value(), 62.0);
        QCOMPARE(build->resists[Element::Lightning].value(), 79.0);
        QCOMPARE(build->resists[Element::Chaos].value(), -10.0);
    }

    void gearOfTheActiveSet()
    {
        const auto build = parsePobBuild(pobfixtures::build);
        QVERIFY(build);
        // not the empty helmet slot, an item that isn't there or a third ring
        QCOMPARE(sortedSlots(*build), (QStringList{u"Belt"_s, u"Gloves"_s, u"Life Flask"_s, u"Offhand"_s,
                                                   u"Weapon"_s, u"Weapon 2"_s}));
        // its second weapon set is the one in use
        QVERIFY(build->swapInUse);
        QCOMPARE(nameIn(*build, u"Weapon"_s), u"Victory Star"_s);
        QCOMPARE(nameIn(*build, u"Offhand"_s), u"Rapture Ward"_s);
        QCOMPARE(nameIn(*build, u"Weapon 2"_s), u"Corpse Song"_s);

        const auto weapon = parseItem(build->gear.value(u"Weapon"_s).text);
        QVERIFY(weapon);
        QCOMPARE(weapon->itemClass, u"Sceptres"_s);
        QCOMPARE(weapon->stats.value(u"# to Level of all Minion Skills"), 4.0);

        const QString gloves = build->gear.value(u"Gloves"_s).text;
        QVERIFY(gloves.contains(u"\n+33 to maximum Life (crafted)\n"_s));
        QVERIFY(!gloves.contains(u"ModRange"_s));
        QCOMPARE(parseItem(gloves)->stats.value(u"# to maximum Life"), 33.0);
    }

    void jewelsOfTheActiveTree()
    {
        const auto build = parsePobBuild(pobfixtures::build);
        QVERIFY(build);
        // in node order, and not the one in a socket it took the point back from
        QCOMPARE(jewelNames(*build), (QStringList{u"Rift Eye"_s, u"Grim Hope"_s}));
        QVERIFY(build->jewels.first().text.startsWith(u"Item Class: Jewels\n"_s));
    }

    void firstWeaponSet()
    {
        const QString xml = QString(pobfixtures::build)
                                .replace(u"useSecondWeaponSet=\"true\""_s, u"useSecondWeaponSet=\"false\""_s);
        const auto build = parsePobBuild(xml);
        QVERIFY(build);
        QVERIFY(!build->swapInUse);
        QCOMPARE(nameIn(*build, u"Weapon"_s), u"Corpse Song"_s);
        QCOMPARE(nameIn(*build, u"Weapon 2"_s), u"Victory Star"_s);
        QCOMPARE(nameIn(*build, u"Offhand 2"_s), u"Rapture Ward"_s);
        QVERIFY(!build->gear.contains(u"Offhand"_s));
    }

    void otherSetsAndTrees()
    {
        const QString first =
            QString(pobfixtures::build).replace(u"activeSpec=\"2\""_s, u"activeSpec=\"1\""_s);
        QCOMPARE(jewelNames(parsePobBuild(first).value()), (QStringList{u"Woe Spark"_s}));

        // ones it doesn't have: Path of Building takes the first item set and the last tree
        const QString missing = QString(pobfixtures::build)
                                    .replace(u"activeItemSet=\"2\""_s, u"activeItemSet=\"7\""_s)
                                    .replace(u"activeSpec=\"2\""_s, u"activeSpec=\"7\""_s);
        const auto build = parsePobBuild(missing);
        QVERIFY(build);
        QCOMPARE(sortedSlots(*build), (QStringList{u"Belt"_s, u"Helmet"_s}));
        QCOMPARE(jewelNames(*build), (QStringList{u"Rift Eye"_s, u"Grim Hope"_s}));
    }

    void littleToGoOn()
    {
        const auto build = parsePobBuild(joined({
            u"<PathOfBuilding2>"_s,
            u"<Build level=\"3\" className=\"Monk\" ascendClassName=\"None\">"_s,
            u"<PlayerStat stat=\"FireResist\" value=\"12\"/>"_s,
            u"<PlayerStat stat=\"ColdResist\" value=\"nan\"/>"_s,
            u"</Build>"_s,
            u"</PathOfBuilding2>"_s,
        }));
        QVERIFY(build);
        QCOMPARE(build->level, 3);
        QCOMPARE(build->className, u"Monk"_s);
        QVERIFY(build->ascendancy.isEmpty());
        QCOMPARE(build->resists[Element::Fire].value(), 12.0);
        QVERIFY(!build->resists[Element::Cold]);
        QVERIFY(!build->resists[Element::Chaos]);
        QVERIFY(build->gear.isEmpty());
        QVERIFY(build->jewels.isEmpty());
    }

    void notABuild()
    {
        const QString poe1 = joined({
            u"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"_s,
            u"<PathOfBuilding>"_s,
            u"<Build level=\"90\" className=\"Witch\" ascendClassName=\"Elementalist\"/>"_s,
            u"</PathOfBuilding>"_s,
        });
        QVERIFY(!parsePobBuild(poe1));
        QVERIFY(isPoe1Build(poe1));
        QVERIFY(!isPoe1Build(pobfixtures::build));
        QVERIFY(!parsePobBuild(u"Rarity: RARE"_s));
        QVERIFY(!parsePobBuild(pobfixtures::build.first(pobfixtures::build.size() / 2)));
    }
};

QTEST_GUILESS_MAIN(TestPobBuild)
#include "tst_pobbuild.moc"
