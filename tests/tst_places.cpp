// SPDX-License-Identifier: MIT
#include "core/parser.h"
#include "core/places.h"
#include "itemtexts.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

Item ring(const QString &name, const QString &mods)
{
    return item(u"Item Class: Rings\nRarity: Rare\n"_s + name + u"\nGold Ring\n--------\n"_s + mods);
}

Item jewel(const QString &name, const QString &mods)
{
    return item(u"Item Class: Jewels\nRarity: Rare\n"_s + name + u"\nSapphire\n--------\n"_s + mods);
}

const Item newRing = item(itemtexts::advancedRing);
const Item strong = ring(u"Grim Band"_s, u"+80 to maximum Life"_s);
const Item weak = ring(u"Doom Band"_s, u"+10 to maximum Life"_s);

Character wearing(const Equipped &gear)
{
    Character c;
    c.gear = gear;
    return c;
}

PobPlacement placement(const QString &slot, const QString &replacing, double dps, double ehp)
{
    return {.slot = slot,
            .label = slot,
            .replacing = replacing,
            .stats = {{u"FullDPS"_s, dps}, {u"TotalEHP"_s, ehp}}};
}

const PobStats before = {{u"FullDPS"_s, 1000}, {u"TotalEHP"_s, 5000}};

} // namespace

class TestPlaces : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void eachRingSlot()
    {
        const Character c = wearing({{u"Ring 1"_s, strong}, {u"Ring 2"_s, weak}});
        // it goes over the weaker one
        const Evaluation e = evaluate(newRing, c);
        QCOMPARE(e.comparison.targetSlot, u"Ring 2"_s);

        const QList<PlaceChange> places = gearPlaces(newRing, c, {}, e.comparison.targetSlot);
        QCOMPARE(places.size(), 2);
        QCOMPARE(places.at(0).slot, u"Ring 1"_s);
        QCOMPARE(places.at(0).wearing, u"Grim Band, Gold Ring"_s);
        QVERIFY(!places.at(0).best);
        QCOMPARE(places.at(1).slot, u"Ring 2"_s);
        QCOMPARE(places.at(1).wearing, u"Doom Band, Gold Ring"_s);
        QVERIFY(places.at(1).best);
        // each as it would be if it were the only one
        QCOMPARE(places.at(0).score, evaluate(newRing, c, {}, u"Ring 1"_s).comparison.delta);
        QCOMPARE(places.at(1).score, e.comparison.delta);
        QVERIFY(places.at(1).score > places.at(0).score);
    }

    void onlyOneBest()
    {
        const Character c = wearing({{u"Ring 1"_s, weak}, {u"Ring 2"_s, strong}});
        const QList<PlaceChange> places = gearPlaces(newRing, c, {}, u"Ring 1"_s);
        QCOMPARE(places.size(), 2);
        QVERIFY(places.at(0).best);
        QVERIFY(!places.at(1).best);
    }

    void needsSomethingInTwo()
    {
        // an empty slot is where it goes, with nothing to choose
        QVERIFY(gearPlaces(newRing, wearing({{u"Ring 1"_s, strong}}), {}, u"Ring 2"_s).isEmpty());
        QVERIFY(gearPlaces(newRing, wearing({}), {}, u"Ring 1"_s).isEmpty());
    }

    void jewelsAndCharms()
    {
        const Item grim = jewel(u"Grim Eye"_s, u"+20 to maximum Life"_s);
        const Item doom = jewel(u"Doom Eye"_s, u"+5 to maximum Life"_s);
        const Item gale = jewel(u"Gale Eye"_s, u"+9 to maximum Life"_s);
        const Item next = jewel(u"Pale Eye"_s, u"+12 to maximum Life"_s);
        const Character c = wearing({{u"Jewel 1"_s, grim}, {u"Jewel 3"_s, doom}, {u"Jewel 4"_s, gale}});
        const QList<PlaceChange> places = gearPlaces(next, c, {}, u"Jewel 3"_s);
        // all six, with the empty ones saying so
        QCOMPARE(places.size(), 6);
        QCOMPARE(places.at(0).wearing, u"Grim Eye, Sapphire"_s);
        QCOMPARE(places.at(1).wearing, QString());
        QCOMPARE(places.at(2).wearing, u"Doom Eye, Sapphire"_s);
        QVERIFY(places.at(2).best);
    }

    void notForOtherGear()
    {
        const Item helmet = item(itemtexts::helmetNew);
        QVERIFY(gearPlaces(helmet, wearing({{u"Helmet"_s, item(itemtexts::helmetOld)}}), {}, u"Helmet"_s)
                    .isEmpty());
        // the verdict doesn't choose between weapon sets
        const auto mace = [](const QString &name) {
            return item(u"Item Class: Two Hand Maces\nRarity: Rare\n"_s + name +
                        u"\nForge Maul\n--------\nPhysical Damage: 50-80\nAttacks per Second: 1.10"_s);
        };
        const Character swapped =
            wearing({{u"Weapon"_s, mace(u"Grim Knell"_s)}, {u"Weapon 2"_s, mace(u"Doom Knell"_s)}});
        QVERIFY(gearPlaces(item(itemtexts::twoHandMace), swapped, {}, u"Weapon"_s).isEmpty());
    }

    void notTheOneYouWear()
    {
        const Character c = wearing({{u"Ring 1"_s, newRing}, {u"Ring 2"_s, weak}});
        QVERIFY(gearPlaces(newRing, c, {}, u"Ring 1"_s).isEmpty());
    }

    void byPathOfBuilding()
    {
        const PobItemEval eval = {.placements = {
                                      placement(u"Ring 1"_s, u"Doom Loop, Gold Ring"_s, 1100, 5000),
                                      placement(u"Ring 2"_s, u"Woe Band, Ruby Ring"_s, 1300, 4900),
                                  }};
        const PobPlacements tried = judgePlacements(before, eval, newRing);
        const QList<PlaceChange> places = pobPlaces(tried);
        QCOMPARE(places.size(), 2);

        QCOMPARE(places.at(0).slot, u"Ring 1"_s);
        QCOMPARE(places.at(0).wearing, u"Doom Loop, Gold Ring"_s);
        QCOMPARE(places.at(0).mode, ScoreMode::PathOfBuilding);
        QCOMPARE(places.at(0).damage, std::optional<double>(10));
        QCOMPARE(places.at(0).ehp, std::optional<double>(0));
        QCOMPARE(places.at(0).score, tried.judgements.at(0).verdict.score);

        QCOMPARE(places.at(1).damage, std::optional<double>(30));
        QCOMPARE(places.at(1).ehp, std::optional<double>(-2));
        // the one the verdict is for
        QCOMPARE(tried.best, qsizetype(1));
        QVERIFY(!places.at(0).best);
        QVERIFY(places.at(1).best);
    }

    void socketsByTheirLabel()
    {
        PobPlacement socket = placement(u"Jewel 61834"_s, u"Grim Eye, Sapphire"_s, 1100, 5000);
        socket.label = u"Socket #3"_s;
        PobPlacement other = placement(u"Jewel 9"_s, u"Doom Eye, Sapphire"_s, 1050, 5000);
        other.label = QString();
        const QList<PlaceChange> places =
            pobPlaces(judgePlacements(before, {.placements = {socket, other}}, newRing));
        QCOMPARE(places.size(), 2);
        QCOMPARE(places.at(0).slot, u"Socket #3"_s);
        // with no label, its own name for it
        QCOMPARE(places.at(1).slot, u"Jewel 9"_s);
    }

    void minionDamage()
    {
        PobPlacement p = placement(u"Ring 1"_s, u"Doom Loop, Gold Ring"_s, 0, 5000);
        p.stats = {{u"MinionDPS"_s, 200}, {u"TotalEHP"_s, 5000}};
        PobPlacement q = p;
        q.slot = q.label = u"Ring 2"_s;
        const PobStats minions = {{u"MinionDPS"_s, 100}, {u"TotalEHP"_s, 5000}};
        const QList<PlaceChange> places =
            pobPlaces(judgePlacements(minions, {.placements = {p, q}}, newRing));
        QCOMPARE(places.size(), 2);
        QVERIFY(places.at(0).minionDamage);
        QCOMPARE(places.at(0).damage, std::optional<double>(100));
    }

    void pathOfBuildingNeedsSomethingInTwo()
    {
        const PobItemEval eval = {.placements = {
                                      placement(u"Ring 1"_s, u"Doom Loop, Gold Ring"_s, 1100, 5000),
                                      placement(u"Ring 2"_s, QString(), 1300, 5000),
                                  }};
        QVERIFY(pobPlaces(judgePlacements(before, eval, newRing)).isEmpty());
        // or in the one place it was tried
        const PobItemEval one = {
            .placements = {placement(u"Ring 1"_s, u"Doom Loop, Gold Ring"_s, 1100, 5000)}};
        QVERIFY(pobPlaces(judgePlacements(before, one, newRing)).isEmpty());
        QVERIFY(pobPlaces(judgePlacements(before, {.error = u"No."_s}, newRing)).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPlaces)
#include "tst_places.moc"
