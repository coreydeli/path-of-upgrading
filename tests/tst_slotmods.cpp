// SPDX-License-Identifier: MIT
#include "core/slotmods.h"

#include <QJsonDocument>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

SlotMods small()
{
    const auto json = QJsonDocument::fromJson(
        R"({"version":"1.2.3","date":"2026-01-02","classes":{"Boots":{
            "# to maximum Life":[[60,120,149],[30,70,99],[1,10,19]],
            "#% increased Movement Speed":[[40,30,35]]}}})");
    return SlotMods::fromJson(json.object());
}

} // namespace

class TestSlotMods : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tiersComeHighestFirst()
    {
        const auto mods = small();
        QCOMPARE(mods.version(), u"1.2.3"_s);
        QCOMPARE(mods.date(), u"2026-01-02"_s);
        const auto tiers = mods.tiers(u"Boots"_s, u"# to maximum Life"_s);
        QCOMPARE(tiers.size(), 3);
        QCOMPARE(tiers.first(), (ModTier{60, 120, 149}));
        QCOMPARE(mods.best(u"Boots"_s, u"# to maximum Life"_s)->high, 149.0);
    }

    void aClassOrStatItCantRollHasNothing()
    {
        const auto mods = small();
        QVERIFY(mods.tiers(u"Boots"_s, u"# to Spirit"_s).isEmpty());
        QVERIFY(!mods.best(u"Gloves"_s, u"# to maximum Life"_s));
        QVERIFY(!mods.bestAt(u"Boots"_s, u"# to Spirit"_s, 80));
    }

    void whatAClassAllows()
    {
        const auto mods = small();
        QVERIFY(mods.allows(u"Boots"_s, u"# to maximum Life"_s));
        QVERIFY(!mods.allows(u"Boots"_s, u"# to Spirit"_s));
        // a class the table doesn't have is allowed anything
        QVERIFY(mods.allows(u"Wands"_s, u"# to Spirit"_s));
        QVERIFY(mods.allows(QString(), u"# to Spirit"_s));
        QVERIFY(SlotMods().allows(u"Boots"_s, u"# to Spirit"_s));
    }

    void keysIgnoreCase()
    {
        const auto mods = small();
        QCOMPARE(mods.tiers(u"Boots"_s, u"# TO MAXIMUM life"_s).size(), 3);
        QVERIFY(mods.allows(u"Boots"_s, u"# to MAXIMUM Life"_s));
    }

    void beltsCantAddDamageButGlovesCan()
    {
        const auto &mods = SlotMods::builtIn();
        QVERIFY(!mods.allows(u"Belts"_s, u"Adds # to # Lightning Damage"_s));
        QVERIFY(!mods.allows(u"Belts"_s, u"Adds # to # Lightning damage to Attacks"_s));
        QVERIFY(mods.allows(u"Gloves"_s, u"Adds # to # Lightning damage to Attacks"_s));
        QVERIFY(mods.allows(u"Rings"_s, u"Adds # to # Physical Damage to Attacks"_s));
        // the Genesis tree's mods aren't rolled on the item
        QVERIFY(!mods.allows(u"Belts"_s, u"Minions Revive #% faster"_s));
        QVERIFY(!mods.allows(u"Rings"_s, u"#% increased Spell Damage"_s));
    }

    void bestForTheItemLevel()
    {
        const auto mods = small();
        QCOMPARE(mods.bestAt(u"Boots"_s, u"# to maximum Life"_s, 45)->high, 99.0);
        QCOMPARE(mods.bestAt(u"Boots"_s, u"# to maximum Life"_s, 80)->high, 149.0);
        QVERIFY(!mods.bestAt(u"Boots"_s, u"#% increased Movement Speed"_s, 10));
    }

    void hybridRangesAreLeftOutWhenThereIsAPlainOne()
    {
        const auto json = QJsonDocument::fromJson(
            "{\"classes\":{\"Boots\":{\"# to maximum Life\":[[70,200,250,1],[60,120,149],[30,70,99]],"
            "\"# to Spirit\":[[50,10,20,1]]}}}");
        const auto mods = SlotMods::fromJson(json.object());
        QVERIFY(mods.tiers(u"Boots"_s, u"# to maximum Life"_s).first().hybrid);
        QCOMPARE(mods.best(u"Boots"_s, u"# to maximum Life"_s)->high, 149.0);
        QCOMPARE(mods.bestAt(u"Boots"_s, u"# to maximum Life"_s, 80)->high, 149.0);
        // with nothing plain, the hybrid is all there is
        QCOMPARE(mods.best(u"Boots"_s, u"# to Spirit"_s)->high, 20.0);
    }

    void classNamesIgnoreCase() { QCOMPARE(small().best(u"boots"_s, u"# to maximum Life"_s)->low, 120.0); }

    void slotsToClasses()
    {
        QCOMPARE(classOfSlot(u"Helmet"_s), u"Helmets"_s);
        QCOMPARE(classOfSlot(u"Body Armour"_s), u"Body Armours"_s);
        QCOMPARE(classOfSlot(u"Weapon"_s), QString());
        QCOMPARE(classOfSlot(u"Jewel"_s), QString());
    }

    void rollsReadPlainly()
    {
        QCOMPARE(rollText(149), u"149"_s);
        QCOMPARE(rollText(35.5), u"35.5"_s);
    }

    void kindsComeFromTheTable()
    {
        const auto json =
            QJsonDocument::fromJson("{\"classes\":{},\"kinds\":{\"Boots\":{\"# to maximum Life\":\"p\","
                                    "\"#% to Cold Resistance\":\"s\",\"# to Spirit\":\"x\"}}}");
        const auto mods = SlotMods::fromJson(json.object());
        QCOMPARE(mods.kind(u"Boots"_s, u"# to maximum Life"_s), AffixKind::Prefix);
        QCOMPARE(mods.kind(u"boots"_s, u"#% to cold resistance"_s), AffixKind::Suffix);
        // not in the table, not a kind it knows, or a class it hasn't got
        QCOMPARE(mods.kind(u"Boots"_s, u"# to Dexterity"_s), AffixKind::Unknown);
        QCOMPARE(mods.kind(u"Boots"_s, u"# to Spirit"_s), AffixKind::Unknown);
        QCOMPARE(mods.kind(u"Gloves"_s, u"# to maximum Life"_s), AffixKind::Unknown);
        QCOMPARE(SlotMods().kind(u"Boots"_s, u"# to maximum Life"_s), AffixKind::Unknown);
    }

    void theShippedTableSaysPrefixOrSuffix()
    {
        const auto &mods = SlotMods::builtIn();
        QCOMPARE(mods.kind(u"Rings"_s, u"#% to Lightning Resistance"_s), AffixKind::Suffix);
        QCOMPARE(mods.kind(u"Rings"_s, u"Adds # to # Physical Damage to Attacks"_s), AffixKind::Prefix);
        QCOMPARE(mods.kind(u"Boots"_s, u"# to maximum Life"_s), AffixKind::Prefix);
        // a prefix and a suffix on different mods of the same class is left unknown
        QCOMPARE(mods.kind(u"Boots"_s, u"# to Stun Threshold"_s), AffixKind::Unknown);
    }

    void theShippedTableIsThere()
    {
        const auto &mods = SlotMods::builtIn();
        QVERIFY(!mods.isEmpty());
        QVERIFY(!mods.version().isEmpty());
        QVERIFY(mods.best(u"Boots"_s, u"# to maximum Life"_s));
    }
};

QTEST_GUILESS_MAIN(TestSlotMods)
#include "tst_slotmods.moc"
