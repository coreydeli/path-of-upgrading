// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/statsources.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

std::optional<double> valueOf(const QList<StatSource> &sources, const QString &key, StatOrigin origin)
{
    for (const StatSource &s : sources) {
        if (s.key == key && s.origin == origin)
            return s.value;
    }
    return std::nullopt;
}

} // namespace

class TestStatSources : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void runesAreKeptApart()
    {
        const Item helmet = parseItem(u"Item Class: Helmets\nRarity: Rare\nA\nB\n--------\nArmour: 300\n"
                                      "--------\n+12% to Fire Resistance (rune)\n--------\n"
                                      "+50 to maximum Life\n+20% to Fire Resistance\n"
                                      "10% increased Armour (rune)\n20% increased Armour"_s)
                                .value();
        const QList<StatSource> s = statSources(helmet);
        QCOMPARE(valueOf(s, u"#% increased Armour"_s, StatOrigin::Mod), 20.0);
        QCOMPARE(valueOf(s, u"#% increased Armour"_s, StatOrigin::Socket), 10.0);
        QCOMPARE(valueOf(s, u"#% to Fire Resistance"_s, StatOrigin::Mod), 20.0);
        QCOMPARE(valueOf(s, u"#% to Fire Resistance"_s, StatOrigin::Socket), 12.0);
        QCOMPARE(valueOf(s, u"# to maximum Life"_s, StatOrigin::Mod), 50.0);
        QCOMPARE(valueOf(s, u"[Base] Armour"_s, StatOrigin::Base), 300.0);
        // the base is listed once, not again as a mod
        QVERIFY(!valueOf(s, u"[Base] Armour"_s, StatOrigin::Mod));
    }

    void implicitsAndSplitMods()
    {
        const Item ring = parseItem(u"Item Class: Rings\nRarity: Rare\nA\nB\n--------\n"
                                    "+7% to Chaos Resistance (implicit)\n--------\n"
                                    "+10% to all Elemental Resistances\n+5% to Cold Resistance"_s)
                              .value();
        const QList<StatSource> s = statSources(ring);
        QCOMPARE(valueOf(s, u"#% to Chaos Resistance"_s, StatOrigin::Implicit), 7.0);
        QCOMPARE(valueOf(s, u"#% to Fire Resistance"_s, StatOrigin::Mod), 10.0);
        QCOMPARE(valueOf(s, u"#% to Cold Resistance"_s, StatOrigin::Mod), 15.0);
        QVERIFY(!valueOf(s, u"#% to all Elemental Resistances"_s, StatOrigin::Mod));
    }

    void statsWithoutLines()
    {
        Item item;
        item.stats = StatMap{{u"# to maximum Life"_s, 50}, {u"[Base] Armour"_s, 300}};
        const QList<StatSource> s = statSources(item);
        QCOMPARE(s.size(), 2);
        QCOMPARE(valueOf(s, u"# to maximum Life"_s, StatOrigin::Mod), 50.0);
        QCOMPARE(valueOf(s, u"[Base] Armour"_s, StatOrigin::Base), 300.0);
    }

    void runeBonusComesOffTheBase()
    {
        const Item gloves =
            parseItem(u"Item Class: Gloves\nRarity: Rare\nDoom Vise\nSorcerer Gloves\n--------\n"
                      "Energy Shield: 280 (augmented)\n--------\n"
                      "18% increased Armour, Evasion and Energy Shield (rune)\n--------\n"
                      "113% increased Energy Shield\n20% increased Energy Shield Recharge Rate\n"
                      "+33 to maximum Life"_s)
                .value();
        const QList<StatSource> s = statSources(gloves);
        // 280 * (100 + 113) / (100 + 113 + 18), and the recharge rate isn't part of it
        QCOMPARE(qRound(baseWithoutSockets(u"[Base] Energy Shield"_s, 280, s) * 100), 25818);
        // DPS isn't scaled by defence mods, and without runes nothing comes off
        QCOMPARE(baseWithoutSockets(u"[Weapon] Total DPS"_s, 150, s), 150.0);
        QCOMPARE(baseWithoutSockets(u"[Base] Energy Shield"_s, 280, {}), 280.0);
    }

    void localModsAreInTheBase()
    {
        const Item gloves = parseItem(u"Item Class: Gloves\nRarity: Rare\nA\nSorcerer Gloves\n--------\n"
                                      "Energy Shield: 280\n--------\n"
                                      "113% increased Energy Shield\n+33 to maximum Energy Shield"_s)
                                .value();
        QVERIFY(isInBase(gloves, u"#% increased Energy Shield"_s));
        QVERIFY(isInBase(gloves, u"# to maximum Energy Shield"_s));
        QVERIFY(!isInBase(gloves, u"# to maximum Life"_s));

        // jewellery has no base defence for them to raise
        const Item ring = parseItem(u"Item Class: Rings\nRarity: Rare\nA\nSapphire Ring\n--------\n"
                                    "+33 to maximum Energy Shield"_s)
                              .value();
        QVERIFY(!isInBase(ring, u"# to maximum Energy Shield"_s));

        const Item sceptre = parseItem(u"Item Class: Sceptres\nRarity: Rare\nA\nRattling Sceptre\n--------\n"
                                       "Spirit: 100\n--------\n30% increased Spirit"_s)
                                 .value();
        QVERIFY(isInBase(sceptre, u"#% increased Spirit"_s));
        QVERIFY(!isInBase(sceptre, u"# to maximum Life"_s));
        QVERIFY(!isInBase(gloves, u"#% increased Spirit"_s));
    }

    void anyKindOfBaseDefence()
    {
        const Item armour = parseItem(u"Item Class: Body Armours\nRarity: Rare\nA\nPlate Vest\n--------\n"
                                      "Armour: 300\n--------\n30% increased Armour"_s)
                                .value();
        QVERIFY(isInBase(armour, u"#% increased Armour"_s));
        const Item evasion = parseItem(u"Item Class: Boots\nRarity: Rare\nA\nLeather Boots\n--------\n"
                                       "Evasion Rating: 200\n--------\n30% increased Evasion Rating"_s)
                                 .value();
        QVERIFY(isInBase(evasion, u"#% increased Evasion Rating"_s));
        // and it needs one to be in
        QVERIFY(!isInBase(armour, u"#% increased Spirit"_s));
    }
};

QTEST_GUILESS_MAIN(TestStatSources)
#include "tst_statsources.moc"
