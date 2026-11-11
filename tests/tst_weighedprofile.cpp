// SPDX-License-Identifier: MIT
#include "core/gearscore.h"
#include "core/parser.h"
#include "core/weighedprofile.h"

#include <QJsonArray>
#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

// Made-up rolls: boots roll life up to 100 and movement speed up to 30.
SlotMods rolls()
{
    const auto tier = [](double high) {
        return QJsonArray{QJsonArray{1, 1, high}};
    };
    return SlotMods::fromJson(QJsonObject{
        {u"classes"_s,
         QJsonObject{
             {u"Boots"_s, QJsonObject{{u"# to maximum Life"_s, tier(100)},
                                      {u"#% increased Movement Speed"_s, tier(30)},
                                      {u"#% to Fire Resistance"_s, tier(40)},
                                      {u"#% to Cold Resistance"_s, tier(40)},
                                      {u"#% to Chaos Resistance"_s, tier(30)}}},
             {u"Rings"_s, QJsonObject{{u"# to maximum Mana"_s, tier(80)}, {u"# to Spirit"_s, tier(10)}}},
         }},
    });
}

BuildProfile startProfile()
{
    BuildProfile p{.id = u"start"_s, .name = u"Start"_s, .summary = {}, .stats = {}};
    p.stats.append({.label = u"Life"_s,
                    .pattern = QRegularExpression(u"^# to maximum Life$"_s),
                    .importance = 8,
                    .cap = 100,
                    .key = u"# to maximum Life"_s,
                    .resist = std::nullopt});
    p.stats.append({.label = u"Armour"_s,
                    .pattern = QRegularExpression(uR"(^\[Base\] Armour$)"_s),
                    .importance = PerSlot::of({{u"Boots"_s, 6}}),
                    .cap = 400,
                    .key = {},
                    .resist = std::nullopt});
    return p;
}

const ProfileStat *statFor(const WeighedProfile &w, const QString &key)
{
    for (const ProfileStat &s : w.profile.stats) {
        if (s.key == key)
            return &s;
    }
    return nullptr;
}

Item boots(const QString &mods)
{
    return parseItem(u"Item Class: Boots\nRarity: Rare\nDusk Treads\nWrapped Boots\n--------\n"_s + mods)
        .value();
}

} // namespace

class TestWeighedProfile : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void theBestStatIsTen()
    {
        // life is worth 0.2 a point of up to 100, speed 0.5 a point of up to 30
        const WeighedProfile w = weighedProfile(
            {{u"Boots"_s, {{u"# to maximum Life"_s, 0.2, 0}, {u"#% increased Movement Speed"_s, 0.5, 0}}}},
            startProfile(), rolls());
        QCOMPARE(w.slots, QStringList{u"Boots"_s});
        // 20 points against 15
        QCOMPARE(statFor(w, u"# to maximum Life"_s)->importance.at(u"Boots"_s), 10);
        QCOMPARE(statFor(w, u"#% increased Movement Speed"_s)->importance.at(u"Boots"_s), 7.5);
        QCOMPARE(statFor(w, u"# to maximum Life"_s)->cap.at(u"Boots"_s), 100);
        QCOMPARE(statFor(w, u"#% increased Movement Speed"_s)->cap.at(u"Boots"_s), 30);
        QCOMPARE(w.profile.id, u"weighed"_s);
    }

    void tinyOnesAreLeftOut()
    {
        const WeighedProfile w = weighedProfile(
            {{u"Boots"_s, {{u"# to maximum Life"_s, 1, 0}, {u"#% increased Movement Speed"_s, 0.001, 0}}}},
            startProfile(), rolls());
        QVERIFY(statFor(w, u"# to maximum Life"_s));
        QVERIFY(!statFor(w, u"#% increased Movement Speed"_s));
    }

    void oneStatFromSeveralMods()
    {
        // implicit and explicit life count once, by the bigger
        const WeighedProfile w = weighedProfile(
            {{u"Boots"_s, {{u"# to maximum Life"_s, 0.1, 0}, {u"# to maximum Life"_s, 0.3, 0}}}},
            startProfile(), rolls());
        QCOMPARE(w.profile.stats.size(), 1 + 3); // and the resistances boots can roll
        QCOMPARE(statFor(w, u"# to maximum Life"_s)->importance.at(u"Boots"_s), 10);
    }

    void noRollNoStat()
    {
        // neither the table nor the answer says what a ring rolls of it
        const WeighedProfile w =
            weighedProfile({{u"Ring"_s, {{u"# to maximum Mana"_s, 1, 0}, {u"# to Evasion Rating"_s, 5, 0}}}},
                           startProfile(), rolls());
        QVERIFY(statFor(w, u"# to maximum Mana"_s));
        QVERIFY(!statFor(w, u"# to Evasion Rating"_s));
    }

    void theAnswersOwnTopCapsWhereTheTableCant()
    {
        // weapons, offhands and jewels have no item class
        const WeighedProfile w = weighedProfile(
            {{u"Weapon"_s, {{u"#% increased Attack Speed"_s, 0.5, 20}, {u"Adds # Fire Damage"_s, 1, 0}}}},
            startProfile(), rolls());
        QCOMPARE(w.slots, QStringList{u"Weapon"_s});
        QCOMPARE(statFor(w, u"#% increased Attack Speed"_s)->cap.at(u"Weapon"_s), 20);
        QVERIFY(!statFor(w, u"Adds # Fire Damage"_s));
        QCOMPARE(statFor(w, u"#% increased Attack Speed"_s)->importance.at(u"Boots"_s), 0);
    }

    void aGroupWithNothingIsNotWeighed()
    {
        const WeighedProfile w = weighedProfile({{u"Boots"_s, {{u"# to maximum Life"_s, 1, 0}}},
                                                 {u"Ring"_s, {{u"# to Evasion Rating"_s, 5, 0}}},
                                                 {u"Belt"_s, {}}},
                                                startProfile(), rolls());
        QCOMPARE(w.slots, QStringList{u"Boots"_s});
        QVERIFY(w.covers(u"Boots"_s));
        QVERIFY(!w.covers(u"Ring"_s));
    }

    void resistancesKeepTheirFlagAndCount()
    {
        // weighed at almost nothing, but boots can roll it
        const WeighedProfile w = weighedProfile(
            {{u"Boots"_s, {{u"# to maximum Life"_s, 1, 0}, {u"#% to Fire Resistance"_s, 0.001, 0}}}},
            startProfile(), rolls());
        const ProfileStat *fire = statFor(w, u"#% to Fire Resistance"_s);
        QVERIFY(fire);
        QCOMPARE(fire->resist, std::optional<Element>(Element::Fire));
        QCOMPARE(fire->label, u"Fire Resistance"_s);
        QCOMPARE(fire->importance.at(u"Boots"_s), resistFloor);
        QCOMPARE(fire->cap.at(u"Boots"_s), 40);
        // and ones the weighing never named
        QVERIFY(statFor(w, u"#% to Cold Resistance"_s));
        // but not a slot's own item with no weighing at all
        QVERIFY(!statFor(w, u"#% to Lightning Resistance"_s));
    }

    void aSmallResistanceIsRaisedToTheFloor()
    {
        // 0.5 a point of up to 40 is 2 out of 10 here
        const WeighedProfile w = weighedProfile(
            {{u"Boots"_s, {{u"# to maximum Life"_s, 1, 0}, {u"#% to Cold Resistance"_s, 0.5, 0}}}},
            startProfile(), rolls());
        QCOMPARE(statFor(w, u"#% to Cold Resistance"_s)->importance.at(u"Boots"_s), resistFloor);
    }

    void aBigResistanceIsKept()
    {
        const WeighedProfile w = weighedProfile(
            {{u"Boots"_s, {{u"# to maximum Life"_s, 0.5, 0}, {u"#% to Cold Resistance"_s, 2, 0}}}},
            startProfile(), rolls());
        QCOMPARE(statFor(w, u"#% to Cold Resistance"_s)->importance.at(u"Boots"_s), 10);
    }

    void resistancesAloneDontMakeAWeighing()
    {
        const WeighedProfile w = weighedProfile({{u"Boots"_s, {}}}, startProfile(), rolls());
        QVERIFY(w.slots.isEmpty());
        QVERIFY(w.profile.stats.isEmpty());
    }

    void labelsFollowTheStartProfile()
    {
        const WeighedProfile w = weighedProfile(
            {{u"Boots"_s, {{u"# to maximum Life"_s, 1, 0}, {u"# to Stun Threshold"_s, 1, 50}}}},
            startProfile(), rolls());
        QCOMPARE(statFor(w, u"# to maximum Life"_s)->label, u"Life"_s);
        QCOMPARE(statFor(w, u"# to Stun Threshold"_s)->label, u"# to Stun Threshold"_s);
    }

    void aStatInSeveralGroupsIsOneRow()
    {
        const WeighedProfile w = weighedProfile({{u"Boots"_s, {{u"# to maximum Life"_s, 1, 0}}},
                                                 {u"Weapon"_s, {{u"# to maximum Life"_s, 1, 50}}}},
                                                startProfile(), rolls());
        int rows = 0;
        for (const ProfileStat &s : w.profile.stats)
            rows += s.key == u"# to maximum Life"_s;
        QCOMPARE(rows, 1);
        const ProfileStat *life = statFor(w, u"# to maximum Life"_s);
        QCOMPARE(life->cap.at(u"Boots"_s), 100);
        QCOMPARE(life->cap.at(u"Weapon"_s), 50);
        QCOMPARE(life->importance.at(u"Ring"_s), 0);
    }

    void itemPropertiesAreTheStarts()
    {
        const QList<ProfileStat> properties = itemPropertyStats(startProfile());
        QCOMPARE(properties.size(), 1);
        QCOMPARE(properties.first().label, u"Armour"_s);
    }

    void saved()
    {
        WeighedProfile w = weighedProfile(
            {{u"Boots"_s, {{u"# to maximum Life"_s, 1, 0}, {u"#% to Fire Resistance"_s, 0.5, 0}}}},
            startProfile(), rolls());
        w.pobVersion = u"0.3.3"_s;
        w.date = u"2026-11-11"_s;
        w.damageWeight = 0.6;
        w.build = u"abc123"_s;
        const std::optional<WeighedProfile> back = weighedProfileFromJson(toJson(w));
        QVERIFY(back);
        QCOMPARE(back->pobVersion, w.pobVersion);
        QCOMPARE(back->date, w.date);
        QCOMPARE(back->damageWeight, 0.6);
        QCOMPARE(back->build, u"abc123"_s);
        QCOMPARE(back->slots, w.slots);
        QCOMPARE(back->profile.stats.size(), w.profile.stats.size());
        for (qsizetype i = 0; i < w.profile.stats.size(); ++i) {
            const ProfileStat &a = w.profile.stats.at(i);
            const ProfileStat &b = back->profile.stats.at(i);
            QCOMPARE(b.label, a.label);
            QCOMPARE(b.key, a.key);
            QCOMPARE(b.resist, a.resist);
            QCOMPARE(b.importance.at(u"Boots"_s), a.importance.at(u"Boots"_s));
            QCOMPARE(b.cap.at(u"Boots"_s), a.cap.at(u"Boots"_s));
            QVERIFY(b.pattern.match(a.key).hasMatch());
        }
    }

    void notSavedIsNone()
    {
        QVERIFY(!weighedProfileFromJson({}));
        QVERIFY(!weighedProfileFromJson({{u"slots"_s, QJsonArray()}}));
    }

    void scoresBootsByTheWeighing()
    {
        // speed counts most for this build, not life as in the start profile
        GearScoreOptions options;
        options.build = u"life-res"_s;
        options.weighed = weighedProfile(
            {{u"Boots"_s, {{u"#% increased Movement Speed"_s, 1, 0}, {u"# to maximum Life"_s, 0.05, 0}}}},
            *buildProfile(u"life-res"), rolls());
        const Item fast = boots(u"+30% increased Movement Speed\n"_s);
        const Item healthy = boots(u"+100 to maximum Life\n"_s);
        const auto weighedFast = gearScore(fast, options);
        const auto weighedHealthy = gearScore(healthy, options);
        QCOMPARE(weighedFast->build, u"weighed"_s);
        QVERIFY(weighedFast->score > weighedHealthy->score);

        options.weighed.reset();
        QCOMPARE(gearScore(fast, options)->build, u"life-res"_s);
    }

    void otherGroupsKeepTheStartProfile()
    {
        GearScoreOptions options;
        options.build = u"life-res"_s;
        options.weighed = weighedProfile({{u"Boots"_s, {{u"#% increased Movement Speed"_s, 1, 0}}}},
                                         *buildProfile(u"life-res"), rolls());
        // boots go by the weighing, rings still by the profile
        QCOMPARE(usefulness(u"Boots"_s, u"#% increased Movement Speed"_s, options), 10.0 / 30);
        QCOMPARE(usefulness(u"Boots"_s, u"# to maximum Mana"_s, options), 0.0);
        QCOMPARE(usefulness(u"Ring 1"_s, u"# to maximum Life"_s, options),
                 usefulness(u"Ring 1"_s, u"# to maximum Life"_s, {.build = u"life-res"_s}));
        QVERIFY(usefulness(u"Ring 1"_s, u"# to maximum Life"_s, options) > 0);

        const QList<KeyStat> top = topStats(u"Boots"_s, 2, options);
        QCOMPARE(top.size(), 1);
        QCOMPARE(top.first().key, u"#% increased Movement Speed"_s);
        QCOMPARE(top.first().cap, 30);

        bool speed = false;
        for (const SearchWeight &w : searchWeights(u"Boots"_s, options))
            speed = speed || w.key == u"#% increased Movement Speed"_s;
        QVERIFY(speed);
    }

    void thePropertiesOfTheProfileStillCount()
    {
        GearScoreOptions options;
        options.build = u"es-caster"_s;
        options.weighed = weighedProfile({{u"Boots"_s, {{u"#% increased Movement Speed"_s, 1, 0}}}},
                                         *buildProfile(u"es-caster"), rolls());
        const Item plain = boots(u"Energy Shield: 400\n"_s);
        QVERIFY(gearScore(plain, options)->score > 0);
    }

    void statWeightsAloneBeatTheWeighing()
    {
        GearScoreOptions options;
        options.build = noBuildProfile.toString();
        options.weighed = weighedProfile({{u"Boots"_s, {{u"#% increased Movement Speed"_s, 1, 0}}}},
                                         *buildProfile(u"life-res"), rolls());
        QVERIFY(!gearScore(boots(u"+30% increased Movement Speed\n"_s), options));
    }
};

QTEST_GUILESS_MAIN(TestWeighedProfile)
#include "tst_weighedprofile.moc"
