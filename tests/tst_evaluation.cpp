// SPDX-License-Identifier: MIT
#include "core/evaluation.h"
#include "core/parser.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

Item amulet(const QString &mods)
{
    return item(u"Item Class: Amulets\nRarity: Rare\nDoom Charm\nGold Amulet\n--------\n"_s + mods);
}

// Your resistances as you'd type them in, with the gear you wear now.
ResistRecord resists(const Equipped &gear, std::optional<double> fire, std::optional<double> cold,
                     std::optional<double> lightning, std::optional<double> chaos)
{
    ResistEntry entry;
    entry.values[Element::Fire] = fire;
    entry.values[Element::Cold] = cold;
    entry.values[Element::Lightning] = lightning;
    entry.values[Element::Chaos] = chaos;
    entry.buffer = 10;
    return snapshotResists(entry, gear, ResistSource::Manual);
}

} // namespace

class TestEvaluation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void gearScoreOptionsFromTheCharacter()
    {
        Character c;
        c.buildProfile = u"es-caster"_s;
        c.tweaks = {{u"Life"_s, 1.5}};
        c.ignored = {IgnoreRule{.key = u"# to maximum Mana"_s}};

        const GearScoreOptions options = gearScoreOptions(c, u"Helmet"_s);
        QCOMPARE(options.build, u"es-caster"_s);
        QCOMPARE(options.tweaks, (ProfileTweaks{{u"Life"_s, 1.5}}));
        QVERIFY(options.ignore(u"# to maximum Mana"_s));
        QVERIFY(!options.ignore(u"# to maximum Life"_s));
        // without resistances there's no telling what you need
        QVERIFY(!options.resistNeed[Element::Fire]);
    }

    void resistancesYouStillNeed()
    {
        Character c;
        c.gear = {{u"Amulet"_s, amulet(u"+20% to Fire Resistance"_s)}};
        c.resists = resists(c.gear, 60, 90, std::nullopt, 0);

        // the amulet's fire resistance goes when another takes its place
        const GearScoreOptions options = gearScoreOptions(c, u"Amulet"_s, {.resistsFirst = false});
        QCOMPARE(options.resistNeed[Element::Fire], 45.0);
        QCOMPARE(options.resistNeed[Element::Cold], 0.0);
        QVERIFY(!options.resistNeed[Element::Lightning]);
        QCOMPARE(options.resistNeed[Element::Chaos], 85.0);
        QCOMPARE(gearScoreOptions(c, u"Helmet"_s).resistNeed[Element::Fire], 25.0);
    }

    void resistancesFirst()
    {
        Character c;
        c.tweaks = {{u"Cold res"_s, 0.5}};
        c.resists = resists({}, 60, 60, 80, 0);

        // those you're short of, unless you tuned them yourself
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s).tweaks,
                 (ProfileTweaks{{u"Fire res"_s, 2}, {u"Cold res"_s, 0.5}}));
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s, {.chaos = true}).tweaks,
                 (ProfileTweaks{{u"Fire res"_s, 2}, {u"Cold res"_s, 0.5}, {u"Chaos res"_s, 2}}));
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s, {.resistsFirst = false}).tweaks,
                 (ProfileTweaks{{u"Cold res"_s, 0.5}}));
    }

    void chaosInoculation()
    {
        Character c;
        c.chaosInoculation = true;
        c.resists = resists({}, 80, 80, 80, 0);
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s, {.chaos = true}).tweaks,
                 (ProfileTweaks{{u"Chaos res"_s, 0}}));

        c.tweaks = {{u"Chaos res"_s, 1}};
        QCOMPARE(gearScoreOptions(c, u"Ring 1"_s).tweaks, (ProfileTweaks{{u"Chaos res"_s, 1}}));
    }
};

QTEST_GUILESS_MAIN(TestEvaluation)
#include "tst_evaluation.moc"
