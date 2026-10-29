// SPDX-License-Identifier: MIT
#include "core/character.h"
#include "core/parser.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

Item withStats(std::initializer_list<StatMap::Entry> stats)
{
    Item i;
    i.stats = StatMap(stats);
    return i;
}

const ResistChange *resistFor(const CharacterChange &c, Element e)
{
    for (const ResistChange &r : c.resists) {
        if (r.element == e)
            return &r;
    }
    return nullptr;
}

} // namespace

class TestCharacter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resistsSpiritAndScore()
    {
        const Equipped worn = {
            {u"Ring 1"_s, item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"
                               "+40% to Cold Resistance\n+50 to maximum Mana"_s)},
            {u"Amulet"_s, item(u"Item Class: Amulets\nRarity: Rare\nB\nSolar Amulet\n--------\n"
                               "+12 to Spirit (implicit)\n--------\n+30% to Fire Resistance"_s)},
        };
        const Item ring = item(u"Item Class: Rings\nRarity: Rare\nC\nIron Ring\n--------\n"
                               "+10% to Cold Resistance\n+90 to maximum Mana\n+20% to Fire Resistance"_s);
        LiveResists resists;
        resists.values[Element::Fire] = 70;
        resists.values[Element::Cold] = 80;
        resists.values[Element::Lightning] = 75;
        resists.values[Element::Chaos] = 0;
        const auto score = [&ring](const Item &i, const QString &) -> std::optional<double> {
            return &i == &ring ? 60 : 40;
        };

        const auto change = changeIfEquipped(worn, ring, {u"Ring 1"_s}, &resists, score);
        QVERIFY(change);
        const ResistChange *cold = resistFor(*change, Element::Cold);
        QCOMPARE(cold->from.value(), 80.0);
        QCOMPARE(cold->to.value(), 50.0);
        QVERIFY(cold->under);
        const ResistChange *fire = resistFor(*change, Element::Fire);
        QCOMPARE(fire->to.value(), 90.0);
        QVERIFY(!fire->under);
        QVERIFY(!resistFor(*change, Element::Lightning));
        QCOMPARE(cold->warning, u"Cold res drops below the cap (50% of 75%)"_s);
        QCOMPARE(fire->warning, QString());

        QCOMPARE(change->totals.size(), 1);
        QCOMPARE(change->totals.first().label, u"Mana"_s);
        QCOMPARE(change->totals.first().from, 50.0);
        QCOMPARE(change->totals.first().to, 90.0);
        QCOMPARE(change->totals.first().delta, 40.0);

        QCOMPARE(change->overall->from, 40.0);
        QCOMPARE(change->overall->to, 50.0);
        QCOMPARE(change->overall->delta, 10.0);
    }

    void thinMarginWarning()
    {
        const QString cold = u"#% to Cold Resistance"_s;
        const Equipped worn = {
            {u"Gloves"_s, withStats({{cold, 45}, {u"#% to Lightning Resistance"_s, 39}})},
            {u"Boots"_s, withStats({{cold, 37}})},
        };
        ResistEntry entry;
        entry.values[Element::Fire] = 78;
        entry.values[Element::Cold] = 112;
        entry.values[Element::Lightning] = 122;
        entry.values[Element::Chaos] = 11;
        const auto live = liveResists(snapshotResists(entry, worn, ResistSource::Pob), worn);

        const auto change = changeIfEquipped(worn, withStats({{cold, 29}}), {u"Gloves"_s}, &*live);
        const QString warning = resistFor(*change, Element::Lightning)->warning;
        QVERIFY(warning.startsWith(u"Lightning res would be only 8% over the cap (-39%)."));
        QVERIFY(warning.contains(u"Path of Building's"));
        // still well over it
        QCOMPARE(resistFor(*change, Element::Cold)->warning, QString());
    }

    void edgesOfTheCap_data()
    {
        QTest::addColumn<double>("from");
        QTest::addColumn<double>("lost");
        QTest::addColumn<bool>("under");
        QTest::addColumn<bool>("wasUnder");
        QTest::addColumn<bool>("thin");
        QTest::addColumn<bool>("wasThin");
        QTest::addColumn<QString>("warning");

        // a cap of 75 and a buffer of 10
        QTest::newRow("onto the cap")
            << 90.0 << 15.0 << false << false << true << false << u"Fire res would be only 0% over the cap"_s;
        QTest::newRow("off the cap") << 75.0 << 1.0 << true << false << false << true
                                     << u"Fire res drops below the cap (74% of 75%)"_s;
        QTest::newRow("onto the buffer") << 100.0 << 15.0 << false << false << false << false << QString();
        QTest::newRow("off the buffer")
            << 85.0 << 5.0 << false << false << true << false << u"Fire res would be only 5% over the cap"_s;
        QTest::newRow("within the buffer") << 80.0 << 2.0 << false << false << true << true << QString();
        QTest::newRow("further under") << 60.0 << 5.0 << true << true << false << false
                                       << u"Fire res goes further under the cap (from 60% to 55%)"_s;
        QTest::newRow("gaining while under") << 60.0 << -5.0 << true << true << false << false << QString();
    }

    void edgesOfTheCap()
    {
        QFETCH(double, from);
        QFETCH(double, lost);
        QFETCH(bool, under);
        QFETCH(bool, wasUnder);
        QFETCH(bool, thin);
        QFETCH(bool, wasThin);
        QFETCH(QString, warning);

        LiveResists resists;
        resists.values[Element::Fire] = from;
        resists.buffer = 10;
        const auto change =
            changeIfEquipped({}, withStats({{u"#% to Fire Resistance"_s, -lost}}), {u"Ring 1"_s}, &resists);
        const ResistChange *fire = resistFor(*change, Element::Fire);
        QCOMPARE(fire->under, under);
        QCOMPARE(fire->wasUnder, wasUnder);
        QCOMPARE(fire->thin, thin);
        QCOMPARE(fire->wasThin, wasThin);
        if (warning.isEmpty())
            QVERIFY(fire->warning.isEmpty());
        else
            QVERIFY2(fire->warning.startsWith(warning), qPrintable(fire->warning));
    }

    void capFromTheResistancesOrTheUsualOne()
    {
        LiveResists resists;
        resists.values[Element::Cold] = 85;
        resists.cap = 80;
        const Item item = withStats({{u"#% to Cold Resistance"_s, 10}});
        QCOMPARE(resistFor(*changeIfEquipped({}, item, {u"Ring 1"_s}, &resists), Element::Cold)->cap, 80.0);
        // the cap for that element, when there is one
        resists.caps[Element::Cold] = 78;
        QCOMPARE(resistFor(*changeIfEquipped({}, item, {u"Ring 1"_s}, &resists), Element::Cold)->cap, 78.0);
        resists.cap = 0;
        resists.caps[Element::Cold] = 0;
        QCOMPARE(resistFor(*changeIfEquipped({}, item, {u"Ring 1"_s}, &resists), Element::Cold)->cap, 75.0);
        QCOMPARE(resistFor(*changeIfEquipped({}, item, {u"Ring 1"_s}), Element::Cold)->cap, 75.0);
    }

    void noOverallScoreWithoutBothSides()
    {
        const Item ring = withStats({{u"# to maximum Life"_s, 30}});
        const auto onlyNew = [&ring](const Item &i, const QString &) -> std::optional<double> {
            return &i == &ring ? std::optional<double>(60) : std::nullopt;
        };
        const Equipped worn = {{u"Ring 1"_s, withStats({{u"# to maximum Life"_s, 10}})}};
        QVERIFY(!changeIfEquipped(worn, ring, {u"Ring 1"_s}, nullptr, onlyNew)->overall);
    }

    void unknownResistsStillShowTheChange()
    {
        const auto change =
            changeIfEquipped({}, withStats({{u"#% to Chaos Resistance"_s, 13}}), {u"Ring 1"_s});
        const ResistChange *chaos = resistFor(*change, Element::Chaos);
        QCOMPARE(chaos->delta, 13.0);
        QVERIFY(!chaos->from);
        QCOMPARE(chaos->warning, QString());
        QVERIFY(!change->overall);
    }

    void twoHanderEmptiesTheOffhand()
    {
        const Equipped worn = {
            {u"Weapon"_s, withStats({{u"# to maximum Life"_s, 30}})},
            {u"Offhand"_s, withStats({{u"# to maximum Life"_s, 50}})},
        };
        const auto change =
            changeIfEquipped(worn, withStats({{u"# to maximum Life"_s, 20}}), {u"Weapon"_s, u"Offhand"_s});
        QCOMPARE(change->totals.first().delta, -60.0);
    }

    void secondWeaponSetLeavesYourResistances()
    {
        const Equipped worn = {{u"Offhand 2"_s, withStats({{u"#% to Fire Resistance"_s, 30}})}};
        const auto change =
            changeIfEquipped(worn, withStats({{u"#% to Cold Resistance"_s, 20}}), {u"Offhand 2"_s});
        QVERIFY(change->resists.isEmpty());
    }

    void jewelleryEnergyShieldAddsUp()
    {
        const QString flat = u"# to maximum Energy Shield"_s;
        QCOMPARE(changeIfEquipped({}, withStats({{flat, 30}}), {u"Amulet"_s})->totals.first().delta, 30.0);
        QVERIFY(changeIfEquipped({}, withStats({{flat, 30}}), {u"Helmet"_s})->totals.isEmpty());
    }

    void overallScore()
    {
        // of the main slots with a gear score
        const Equipped worn = {
            {u"Helmet"_s, Item()},
            {u"Ring 1"_s, Item()},
            {u"Belt"_s, Item()},
            {u"Jewel 1"_s, Item()},
        };
        const QHash<QString, double> scores = {{u"Helmet"_s, 41}, {u"Ring 1"_s, 50}, {u"Jewel 1"_s, 10}};
        const auto score = [&scores](const Item &, const QString &slot) -> std::optional<double> {
            if (!scores.contains(slot))
                return std::nullopt;
            return scores.value(slot);
        };
        QCOMPARE(overallGearScore(worn, score), 46.0);

        // and none
        QCOMPARE(overallGearScore({{u"Belt"_s, Item()}, {u"Jewel 1"_s, Item()}}, score), std::nullopt);
        QCOMPARE(overallGearScore({}, score), std::nullopt);
    }
};

QTEST_GUILESS_MAIN(TestCharacter)
#include "tst_character.moc"
