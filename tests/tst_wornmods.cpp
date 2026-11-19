// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/wornmods.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString rare = u"Item Class: Gloves\nRarity: Rare\nDoom Vise\nSorcerer Gloves\n--------\n"
                     "Energy Shield: 280\n--------\n"
                     "{ Rune Modifier }\n+12% to Cold Resistance (rune)\n--------\n"
                     "{ Implicit Modifier }\n+5% to Fire Resistance (implicit)\n--------\n"
                     "{ Suffix Modifier \"of the Squall\" (Tier: 3) }\n+39(36-40)% to Lightning Resistance\n"
                     "{ Prefix Modifier \"Hale\" (Tier: 3) }\n+33(33-41) to maximum Life (fractured)\n"
                     "{ Prefix Modifier \"Shining\" (Tier: 2) }\n75(68-79)% increased Energy Shield"_s;

QStringList texts(const QList<WornMod> &mods)
{
    QStringList out;
    for (const WornMod &mod : mods)
        out << mod.group + u": "_s + mod.text;
    return out;
}

} // namespace

class TestWornMods : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void modsComeInTheGamesOrder()
    {
        const auto it = parseItem(rare);
        QCOMPARE(
            texts(wornMods(*it)),
            (QStringList{u"Implicit: +5% to Fire Resistance"_s, u"Prefixes: +33 to maximum Life"_s,
                         u"Prefixes: 75% increased Energy Shield"_s,
                         u"Suffixes: +39% to Lightning Resistance"_s, u"Runes: +12% to Cold Resistance"_s}));
    }

    void fracturedModsAreMarked()
    {
        const auto it = parseItem(rare);
        const QList<WornMod> mods = wornMods(*it);
        for (const WornMod &mod : mods)
            QCOMPARE(mod.fractured, mod.text.contains(u"Life"));
        QVERIFY(hasFracturedMod(*it));
        QVERIFY(!hasFracturedMod(*parseItem(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"
                                            "+5 to Dexterity"_s)));
    }

    void aModTheTableCantPlaceIsUnderItsOwnHeading()
    {
        // Gloves can't roll movement speed, so a plain copy can't say what it is
        const auto it = parseItem(u"Item Class: Gloves\nRarity: Rare\nA\nSorcerer Gloves\n--------\n"
                                  "+39% to Lightning Resistance\n10% increased Movement Speed"_s);
        QCOMPARE(texts(wornMods(*it)), (QStringList{u"Suffixes: +39% to Lightning Resistance"_s,
                                                    u"Other mods: 10% increased Movement Speed"_s}));
    }

    void anEnchantComesAfterTheRunes()
    {
        const auto it = parseItem(u"Item Class: Boots\nRarity: Rare\nA\nIron Greaves\n--------\n"
                                  "20% increased Movement Speed (enchant)\n--------\n"
                                  "+12% to Cold Resistance (rune)\n--------\n+80 to maximum Life"_s);
        QCOMPARE(texts(wornMods(*it)),
                 (QStringList{u"Prefixes: +80 to maximum Life"_s, u"Runes: +12% to Cold Resistance"_s,
                              u"Enchants: 20% increased Movement Speed"_s}));
    }

    void uniqueModsAreJustMods()
    {
        const auto it = parseItem(u"Item Class: Belts\nRarity: Unique\nCoil of Ash\nRawhide Belt\n--------\n"
                                  "+40 to maximum Life\n+30% to Fire Resistance"_s);
        QCOMPARE(texts(wornMods(*it)),
                 (QStringList{u"Mods: +40 to maximum Life"_s, u"Mods: +30% to Fire Resistance"_s}));
    }

    void propertiesAreNotMods()
    {
        const auto it = parseItem(u"Item Class: Gloves\nRarity: Rare\nA\nSorcerer Gloves\n--------\n"
                                  "Energy Shield: 280\n--------\n+80 to maximum Life"_s);
        QCOMPARE(wornMods(*it).size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestWornMods)
#include "tst_wornmods.moc"
