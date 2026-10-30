// SPDX-License-Identifier: MIT
#include "core/buildprofiles.h"
#include "core/gearscore.h"
#include "core/parser.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

const ProfileStat *statLabelled(const BuildProfile &profile, const QString &label)
{
    for (const ProfileStat &s : profile.stats) {
        if (s.label == label)
            return &s;
    }
    return nullptr;
}

Item item(const QString &text)
{
    return parseItem(text).value();
}

const SearchWeight *weightLabelled(const QList<SearchWeight> &weights, const QString &label)
{
    for (const SearchWeight &w : weights) {
        if (w.label == label)
            return &w;
    }
    return nullptr;
}

const QString greatHelm = u"Item Class: Helmets\nRarity: Rare\nGreat Crown\nAncestral Tiara\n--------\n"
                          "Energy Shield: 520\n--------\n+3 to Level of all Minion Skills\n"
                          "+42% to Cold Resistance\n+40% to Fire Resistance\n+90 to maximum Life\n"
                          "+30% to Chaos Resistance\n80% increased Energy Shield"_s;

} // namespace

class TestGearScore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void profiles()
    {
        QCOMPARE(buildProfile(u"")->id, u"life-res"_s);
        QCOMPARE(buildProfile(u"minion-es")->id, u"minion-es"_s);
        // the ones picked by name, which characters have saved
        QCOMPARE(buildProfile(defaultBuildProfile)->id, u"life-res"_s);
        QCOMPARE(buildProfile(energyShieldProfile)->id, u"es-caster"_s);
        QCOMPARE(buildProfile(minionProfile)->id, u"minion-es"_s);
        QVERIFY(!buildProfile(u"nothing like it"));
        QCOMPARE(buildProfiles().size(), 3);
        for (const BuildProfile &p : buildProfiles()) {
            for (const ProfileStat &s : p.stats)
                QVERIFY2(s.pattern.isValid(), qPrintable(p.id + u": "_s + s.label));
        }
    }

    void noProfile()
    {
        QVERIFY(!buildProfile(noBuildProfile));
        const GearScoreOptions weightsOnly{.build = noBuildProfile.toString()};
        QVERIFY(!gearScore(item(greatHelm), weightsOnly));
    }

    void tweakableStats()
    {
        const QList<TweakableStat> stats = pou::tweakableStats(*buildProfile(u"es-caster"));
        QStringList labels;
        for (const TweakableStat &s : stats)
            labels << s.label;
        QCOMPARE(labels.first(), u"Spell skill levels"_s);
        QCOMPARE(labels.size(), 16);
        QCOMPARE(labels.removeDuplicates(), 0);

        const auto groupsOf = [&stats](const QString &label) {
            for (const TweakableStat &s : stats) {
                if (s.label == label)
                    return s.groups;
            }
            return QStringList{u"none"_s};
        };
        QCOMPARE(groupsOf(u"Movement speed"_s), QStringList{u"Boots"_s});
        // armour's Energy Shield and jewellery's, in the order of the slots
        QCOMPARE(groupsOf(u"Energy Shield"_s),
                 (QStringList{u"Offhand"_s, u"Helmet"_s, u"Body Armour"_s, u"Gloves"_s, u"Boots"_s, u"Belt"_s,
                              u"Amulet"_s, u"Ring"_s}));
        QCOMPARE(groupsOf(u"Life"_s), ratedGroups().mid(1, 8));
    }

    void perSlotValues()
    {
        const ProfileStat *ms = statLabelled(*buildProfile(u"life-res"), u"Movement speed"_s);
        QVERIFY(ms);
        QCOMPARE(ms->importance.at(u"Boots"_s), 10.0);
        QCOMPARE(ms->importance.at(u"Helmet"_s), 0.0);
        QCOMPARE(ms->cap.at(u"Boots"_s), 35.0);
        QVERIFY(ms->pattern.match(u"#% increased Movement Speed"_s).hasMatch());
        QVERIFY(!ms->pattern.match(u"#% increased Movement Speed if you've Hit Recently"_s).hasMatch());

        QCOMPARE(resistLabel(Element::Cold), u"Cold res"_s);
        const ProfileStat *cold = statLabelled(*buildProfile(u"life-res"), u"Cold res"_s);
        QCOMPARE(cold->resist, Element::Cold);
        QCOMPARE(cold->key, u"#% to Cold Resistance"_s);
        QCOMPARE(cold->importance.at(u"Jewel"_s), 0.0);
        QCOMPARE(cold->importance.at(u"Ring"_s), 5.0);
    }

    void patternsCoverSeveralMods()
    {
        const BuildProfile &minions = *buildProfile(u"minion-es");
        const ProfileStat *speed = statLabelled(minions, u"Minion speed"_s);
        QVERIFY(speed->pattern.match(u"Allies in your Presence have #% increased Attack Speed"_s).hasMatch());
        QVERIFY(speed->pattern.match(u"Minions have #% increased Cast Speed"_s).hasMatch());
        QVERIFY(!speed->pattern.match(u"#% increased Attack Speed"_s).hasMatch());
        const ProfileStat *damage = statLabelled(minions, u"Minion damage"_s);
        QVERIFY(
            damage->pattern.match(u"Minions deal #% increased Damage if you've Hit Recently"_s).hasMatch());
        const ProfileStat *defence = statLabelled(minions, u"Minion defence"_s);
        QVERIFY(defence->pattern.match(u"Minions have #% increased maximum Life"_s).hasMatch());
        QVERIFY(
            defence->pattern.match(u"Minions gain #% of their maximum Life as Extra maximum Energy Shield"_s)
                .hasMatch());
    }

    void ratesForTheBuild()
    {
        const GearScoreOptions minions{.build = u"minion-es"_s};
        const auto great = gearScore(item(greatHelm), minions);
        QVERIFY(great && great->isRated());
        QCOMPARE(great->score, 89);
        QCOMPARE(great->grade, u"S"_s);
        QCOMPARE(great->ideal, 38.0);
        // local Energy Shield % is already in the base value, so it isn't wasted
        QVERIFY(great->wasted.isEmpty());
        QCOMPARE(great->contributions.first().label, u"Minion skill levels"_s);
        QCOMPARE(great->contributions.first().quality, 115);

        const auto junk = gearScore(item(u"Item Class: Helmets\nRarity: Rare\nBad Hat\nAncestral Tiara\n"
                                         "--------\nEnergy Shield: 150\n--------\n"
                                         "Adds 1 to 4 Physical Damage to Attacks\n+10 to Strength\n"
                                         "12% increased Stun Threshold"_s),
                                    minions);
        QVERIFY(junk->score < 30);
        QCOMPARE(junk->grade, u"D"_s);
        QCOMPARE(junk->missing, QStringList{u"Minion skill levels"_s});
        QCOMPARE(junk->wasted.size(), 2);
    }

    void ignoredStatsLeaveTheIdeal()
    {
        GearScoreOptions noRes{.build = u"minion-es"_s};
        noRes.ignore = [](const QString &key) {
            return key.contains(u"Resistance");
        };
        const auto g = gearScore(item(greatHelm), noRes);
        // mostly resistances, so it's worth less to you, but it doesn't drop to nothing
        QCOMPARE(g->score, 71);
        QCOMPARE(g->ideal, 31.5);
    }

    void missingAndUnrated()
    {
        const GearScoreOptions minions{.build = u"minion-es"_s};
        const auto boots =
            gearScore(item(u"Item Class: Boots\nRarity: Rare\nX\nY\n--------\nEnergy Shield: 100\n--------\n"
                           "+30 to maximum Life"_s),
                      minions);
        QCOMPARE(boots->missing.value(0), u"Movement speed"_s);

        const auto belt =
            gearScore(item(u"Item Class: Belts\nRarity: Unique\nMageblood\nHeavy Belt\n--------\n"
                           "+50 to maximum Life"_s));
        QCOMPARE(belt->unrated, GearScore::Unrated::Unique);
        QVERIFY(belt->grade.isEmpty());
        QCOMPARE(belt->build, u"life-res"_s);

        QVERIFY(!gearScore(item(greatHelm), {.build = u"no such build"_s}));
    }

    void resistancesOnlyCountUpToTheCap()
    {
        const Item ring = item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"
                               "+40% to Fire Resistance\n+30% to Cold Resistance\n"
                               "+25% to Lightning Resistance\n+60 to maximum Life"_s);
        const auto open = gearScore(ring);
        GearScoreOptions capped;
        capped.resistNeed[Element::Fire] = 20;
        capped.resistNeed[Element::Cold] = 0;
        // under a quarter of an excellent roll short, so it drops out entirely
        capped.resistNeed[Element::Lightning] = 5;
        const auto g = gearScore(ring, capped);
        QVERIFY(g->score < open->score);
        QCOMPARE(g->resistNote,
                 u"Fire res: you only need 20 more to reach the cap, so the rest doesn't count. "
                 "Cold res: you're already capped, so it doesn't count. "
                 "Lightning res: you only need 5 more to reach the cap, so it isn't counted."_s);
    }

    void tweaksScaleImportance()
    {
        const Item boots = item(u"Item Class: Boots\nRarity: Rare\nX\nY\n--------\n"
                                "+30 to maximum Life"_s);
        GearScoreOptions noLife;
        noLife.tweaks.insert(u"Life"_s, 0);
        const auto g = gearScore(boots, noLife);
        QCOMPARE(g->score, 0);
        QVERIFY(g->contributions.isEmpty());
    }

    void searchWeightsForASlot()
    {
        GearScoreOptions options{.build = u"minion-es"_s};
        options.ignore = [](const QString &key) {
            return key == u"# to maximum Life";
        };
        options.resistNeed[Element::Fire] = 0;
        options.resistNeed[Element::Cold] = 20;
        const QList<SearchWeight> weights = searchWeights(u"Ring 2"_s, options);
        QVERIFY(!weightLabelled(weights, u"Life"_s));
        QVERIFY(!weightLabelled(weights, u"Fire res"_s));
        QVERIFY(!weightLabelled(weights, u"Minion skill levels"_s)); // can't roll on rings
        const SearchWeight *damage = weightLabelled(weights, u"Minion damage"_s);
        QCOMPARE(damage->weight, 7.0 / 25);
        QCOMPARE(damage->importance, 7.0);
        const SearchWeight *cold = weightLabelled(weights, u"Cold res"_s);
        QCOMPARE(cold->need, 20.0);
        QCOMPARE(weightLabelled(weights, u"Lightning res"_s)->need, std::nullopt);
    }

    void topStatsAndUsefulness()
    {
        const QList<KeyStat> boots = topStats(u"Boots"_s);
        QCOMPARE(boots.size(), 2);
        QCOMPARE(boots[0].label, u"Movement speed"_s);
        QCOMPARE(boots[1].label, u"Life"_s);
        QCOMPARE(boots[0].cap, 35.0);
        QCOMPARE(usefulness(u"Ring 1"_s, u"# to maximum Life"_s), 0.07);
        QCOMPARE(usefulness(u"Weapon"_s, u"# to maximum Life"_s), 0.0);
        QCOMPARE(usefulness(u"Ring 1"_s, u"nothing #"_s), 0.0);

        QCOMPARE(usefulness(u"Ring 1"_s, u"# to maximum Life"_s, {.tweaks = {{u"Life"_s, 2}}}), 0.14);
        QCOMPARE(usefulness(u"Ring 1"_s, u"#% to Chaos Resistance"_s, {.tweaks = {{u"Chaos res"_s, 0}}}),
                 0.0);
        QCOMPARE(usefulness(u"Amulet"_s, u"# to maximum Mana"_s, {.build = u"es-caster"_s}), 0.02);
    }

    void gradesChangeAtTheirBoundaries_data()
    {
        QTest::addColumn<int>("score");
        QTest::addColumn<QString>("grade");
        QTest::newRow("100") << 100 << u"S"_s;
        QTest::newRow("85") << 85 << u"S"_s;
        QTest::newRow("84") << 84 << u"A"_s;
        QTest::newRow("70") << 70 << u"A"_s;
        QTest::newRow("69") << 69 << u"B"_s;
        QTest::newRow("55") << 55 << u"B"_s;
        QTest::newRow("54") << 54 << u"C"_s;
        QTest::newRow("40") << 40 << u"C"_s;
        QTest::newRow("39") << 39 << u"D"_s;
        QTest::newRow("0") << 0 << u"D"_s;
    }

    void gradesChangeAtTheirBoundaries()
    {
        QFETCH(int, score);
        QFETCH(QString, grade);
        QCOMPARE(gearGrade(score), grade);
    }

    void everyWantedStatHasACap()
    {
        for (const BuildProfile &p : buildProfiles()) {
            for (const ProfileStat &s : p.stats) {
                for (const QString &group : ratedGroups()) {
                    if (s.importance.at(group) > 0)
                        QVERIFY2(s.cap.at(group) > 0,
                                 qPrintable(p.id + u" / "_s + s.label + u" / "_s + group));
                }
            }
        }
    }

    void flasksAndCharmsAreNotRated()
    {
        for (const QString &itemClass : {u"Life Flasks"_s, u"Mana Flasks"_s, u"Charms"_s}) {
            const auto g = gearScore(
                item(u"Item Class: "_s + itemClass + u"\nRarity: Magic\nA\n--------\n+10 to maximum Life"_s));
            QVERIFY2(g, qPrintable(itemClass));
            QCOMPARE(g->unrated, GearScore::Unrated::FlaskOrCharm);
        }
    }

    void unidentifiedItemsAreNotRated()
    {
        const auto g = gearScore(item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"
                                      "Unidentified"_s));
        QCOMPARE(g->unrated, GearScore::Unrated::Unidentified);
        QVERIFY(g->grade.isEmpty());
    }

    void itemsWithoutASlotAreNotScored()
    {
        const Item currency = item(
            u"Item Class: Stackable Currency\nRarity: Currency\nChaos Orb\n--------\nStack Size: 3/20"_s);
        QVERIFY(currency.slot.isEmpty());
        QVERIFY(!gearScore(currency));
    }

    void jewelsAreMeasuredAgainstFourStats()
    {
        // eight of the minion profile's stats count on a jewel, and the best four make the ideal
        const auto g = gearScore(item(u"Item Class: Jewels\nRarity: Rare\nA\nCobalt Jewel\n--------\n"
                                      "Minions deal 20% increased Damage"_s),
                                 {.build = u"minion-es"_s});
        QCOMPARE(g->ideal, 24.0);
    }

    void theIdealIsTheBestSixOrSevenStats()
    {
        const auto ring = gearScore(item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring"_s));
        QCOMPARE(ring->ideal, 28.0);
        const auto helmet = gearScore(item(u"Item Class: Helmets\nRarity: Rare\nA\nIron Hat"_s));
        QCOMPARE(helmet->ideal, 42.0);
    }

    void aSlotTheBuildHasNoUseForScoresNothing()
    {
        const auto g = gearScore(item(u"Item Class: Jewels\nRarity: Rare\nA\nCobalt Jewel\n--------\n"
                                      "+20 to maximum Life"_s));
        QCOMPARE(g->score, 0);
        QCOMPARE(g->grade, u"D"_s);
        QCOMPARE(g->ideal, 1.0);
    }

    void scoresStopAtOneHundred()
    {
        const auto g = gearScore(item(u"Item Class: Jewels\nRarity: Rare\nA\nCobalt Jewel\n--------\n"
                                      "20% increased Spell Damage\n14% increased maximum Energy Shield\n"
                                      "8% increased Cast Speed\n20% increased Critical Hit Chance"_s),
                                 {.build = u"es-caster"_s});
        QCOMPARE(g->score, 100);
        QCOMPARE(g->grade, u"S"_s);
    }

    void ignoringEverythingKeepsStatsWithoutAKey()
    {
        // a helmet's defences are several mods, so there's no one key to ignore them by
        GearScoreOptions all;
        all.ignore = [](const QString &) {
            return true;
        };
        const auto g = gearScore(item(greatHelm), all);
        QCOMPARE(g->score, 0);
        QCOMPARE(g->ideal, 19.0);
    }

    void modsThatNetToNothingAreNotWasted()
    {
        const auto g = gearScore(item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"
                                      "20% increased Stun Threshold\n20% reduced Stun Threshold\n"
                                      "15% reduced Flask Charges Gained"_s));
        QVERIFY(g->wasted.isEmpty());
    }

    void unwantedImplicitsAreWastedButRunesAndBasesAreNot()
    {
        const auto g = gearScore(item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"
                                      "20% increased Stun Threshold (implicit)\n--------\n"
                                      "15% increased Flask Charges Gained\n--------\n"
                                      "12% increased Light Radius (rune)"_s));
        QCOMPARE(g->wasted,
                 (QStringList{u"#% increased Stun Threshold"_s, u"#% increased Flask Charges Gained"_s}));
    }

    void resistanceNeedsAreCheckedAtTheirEdges()
    {
        const Item ring = item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"
                               "+20% to Fire Resistance\n+60 to maximum Life"_s);
        const auto open = gearScore(ring);

        // a quarter of an excellent roll short is still worth looking for
        GearScoreOptions options;
        options.resistNeed[Element::Fire] = 11.25;
        QCOMPARE(gearScore(ring, options)->ideal, open->ideal);
        options.resistNeed[Element::Fire] = 11.2;
        QVERIFY(gearScore(ring, options)->ideal < open->ideal);

        // a roll that just covers the need has nothing left over
        options.resistNeed[Element::Fire] = 20;
        const auto exact = gearScore(ring, options);
        QVERIFY(exact->resistNote.isEmpty());
        QCOMPARE(exact->score, open->score);
        options.resistNeed[Element::Fire] = 19;
        QVERIFY(!gearScore(ring, options)->resistNote.isEmpty());
    }

    void aResistanceIsNotedOnce()
    {
        const Item ring = item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"
                               "+10% to Fire Resistance (implicit)\n--------\n+30% to Fire Resistance"_s);
        GearScoreOptions options;
        options.resistNeed[Element::Fire] = 5;
        QCOMPARE(gearScore(ring, options)->resistNote,
                 u"Fire res: you only need 5 more to reach the cap, so it isn't counted."_s);
    }

    void runesAndConditionalModsCountForLess()
    {
        const auto pointsFor = [](const QString &mod) {
            const Item ring = item(u"Item Class: Rings\nRarity: Rare\nA\nIron Ring\n--------\n"_s + mod);
            return gearScore(ring, {.build = u"minion-es"_s})->contributions.first().points;
        };
        QCOMPARE(pointsFor(u"Minions deal 20% increased Damage"_s), 5.6);
        QCOMPARE(pointsFor(u"Minions deal 20% increased Damage (rune)"_s), 2.8);
        QCOMPARE(pointsFor(u"Minions deal 20% increased Damage if you've Hit Recently"_s), 3.4);
    }

    void aStatCountsForAboutOneExcellentRoll()
    {
        // the rune comes after a roll that's already past excellent, so it adds nothing
        const auto g = gearScore(item(u"Item Class: Body Armours\nRarity: Rare\nA\nB\n--------\n"
                                      "+150 to maximum Life\n--------\n+50 to maximum Life (rune)"_s));
        QCOMPARE(g->contributions.size(), 1);
        QCOMPARE(g->contributions.first().quality, 115);
    }

    void missingListsTheThreeBestStatsTheItemLacks()
    {
        const auto bare = gearScore(item(u"Item Class: Body Armours\nRarity: Rare\nA\nB"_s));
        QCOMPARE(bare->missing, (QStringList{u"Armour / Evasion"_s, u"Energy Shield"_s, u"Life"_s}));

        // a stat worth 5 is listed, and one worth less isn't
        const Item armoured = item(u"Item Class: Body Armours\nRarity: Rare\nA\nB\n--------\n"
                                   "Armour: 900\nEnergy Shield: 900\n--------\n+80 to maximum Life\n"
                                   "+30% to Fire Resistance\n+30% to Cold Resistance"_s);
        QCOMPARE(gearScore(armoured)->missing, QStringList{u"Lightning res"_s});
    }

    void topStatsIncludeThoseWorthFour()
    {
        QStringList labels;
        for (const KeyStat &s : topStats(u"Ring"_s, 10, {.build = u"minion-es"_s}))
            labels << s.label;
        // not the resistances, which are worth 4 here, nor the stats with no one key
        QCOMPARE(labels, (QStringList{u"Minion damage"_s, u"Energy Shield"_s, u"Life"_s}));
    }
};

QTEST_GUILESS_MAIN(TestGearScore)
#include "tst_gearscore.moc"
