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

    void bestForTheItemLevel()
    {
        const auto mods = small();
        QCOMPARE(mods.bestAt(u"Boots"_s, u"# to maximum Life"_s, 45)->high, 99.0);
        QCOMPARE(mods.bestAt(u"Boots"_s, u"# to maximum Life"_s, 80)->high, 149.0);
        QVERIFY(!mods.bestAt(u"Boots"_s, u"#% increased Movement Speed"_s, 10));
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
