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

    void jewelleryEnergyShieldAddsUp()
    {
        const QString flat = u"# to maximum Energy Shield"_s;
        QCOMPARE(changeIfEquipped({}, withStats({{flat, 30}}), {u"Amulet"_s})->totals.first().delta, 30.0);
        QVERIFY(changeIfEquipped({}, withStats({{flat, 30}}), {u"Helmet"_s})->totals.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCharacter)
#include "tst_character.moc"
