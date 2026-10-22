// SPDX-License-Identifier: MIT
#include "core/resists.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

Item withStats(std::initializer_list<StatMap::Entry> stats)
{
    Item item;
    item.stats = StatMap(stats);
    return item;
}

const QString cold = u"#% to Cold Resistance"_s;
const QString lightning = u"#% to Lightning Resistance"_s;

} // namespace

class TestResists : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void followGearSwaps()
    {
        const Equipped worn = {
            {u"Gloves"_s, withStats({{cold, 45}, {lightning, 39}})},
            {u"Boots"_s, withStats({{cold, 37}})},
        };
        ResistEntry entry;
        entry.values[Element::Fire] = 78;
        entry.values[Element::Cold] = 112;
        entry.values[Element::Lightning] = 122;
        entry.values[Element::Chaos] = 11;
        const ResistRecord record = snapshotResists(entry, worn, ResistSource::Pob);
        QCOMPARE(liveResists(record, worn)->values[Element::Lightning].value(), 122.0);

        Equipped swapped = worn;
        swapped[u"Gloves"_s] = withStats({{cold, 29}});
        const auto live = liveResists(record, swapped);
        QCOMPARE(live->values[Element::Lightning].value(), 83.0);
        QCOMPARE(live->values[Element::Cold].value(), 96.0);
        QCOMPARE(live->values[Element::Fire].value(), 78.0);
        QCOMPARE(live->buffer, 15.0);
    }

    void onlyMainSlotsCount()
    {
        ResistEntry entry;
        entry.values[Element::Cold] = 50;
        const ResistRecord record = snapshotResists(entry, {}, ResistSource::Manual);
        const auto live = liveResists(record, {{u"Weapon 2"_s, withStats({{cold, 30}})}});
        QCOMPARE(live->values[Element::Cold].value(), 50.0);
    }

    void recopyingAnItemKeepsTotals()
    {
        Item before;
        before.name = u"Mageblood"_s;
        before.baseType = u"Utility Belt"_s;
        Item after = before;
        after.stats.set(cold, 45);

        ResistEntry entry;
        entry.values[Element::Cold] = 112;
        ResistRecord record = snapshotResists(entry, {{u"Belt"_s, before}}, ResistSource::Pob);
        record = rebaseResists(record, before, after);
        QCOMPARE(liveResists(record, {{u"Belt"_s, after}})->values[Element::Cold].value(), 112.0);

        Item other = after;
        other.name = u"Something Else"_s;
        QCOMPARE(rebaseResists(record, before, other), record);
    }

    void maximumResistanceRaisesTheCap()
    {
        const Item ruby = withStats({{u"#% to Maximum Fire Resistance"_s, 5}});
        ResistEntry entry;
        entry.values[Element::Fire] = 80;
        const auto live = liveResists(snapshotResists(entry, {}, ResistSource::Game), {{u"Belt"_s, ruby}});
        QCOMPARE(live->caps[Element::Fire], 80.0);
        QCOMPARE(live->caps[Element::Cold], 75.0);
        QCOMPARE(live->buffer, 5.0);
        QVERIFY(!live->values[Element::Cold]);
    }

    void nothingEntered() { QVERIFY(!liveResists(snapshotResists({}, {}, ResistSource::Manual), {})); }

    void chosenBufferIsKept()
    {
        ResistEntry first;
        first.values[Element::Fire] = 75;
        first.buffer = 20;
        const ResistRecord a = snapshotResists(first, {}, ResistSource::Manual);

        ResistEntry second;
        second.values[Element::Fire] = 76;
        const ResistRecord b = snapshotResists(second, {}, ResistSource::Pob, &a);
        QCOMPARE(b.buffer, 20.0);
        QVERIFY(b.bufferSet);

        const ResistRecord c = snapshotResists(second, {}, ResistSource::Pob);
        QCOMPARE(c.buffer, 15.0);
        QVERIFY(!c.bufferSet);
    }

    void jsonRoundTrip()
    {
        ResistEntry entry;
        entry.values[Element::Fire] = 78;
        entry.values[Element::Chaos] = -10;
        entry.cap = 76;
        const ResistRecord record =
            snapshotResists(entry, {{u"Gloves"_s, withStats({{cold, 45}})}}, ResistSource::Pob);
        QCOMPARE(resistRecordFromJson(toJson(record)), record);
    }
};

QTEST_GUILESS_MAIN(TestResists)
#include "tst_resists.moc"
