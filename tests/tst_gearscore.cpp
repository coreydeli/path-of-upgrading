// SPDX-License-Identifier: MIT
#include "core/buildprofiles.h"

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

} // namespace

class TestGearScore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void profiles()
    {
        QCOMPARE(buildProfile(u"")->id, u"life-res"_s);
        QCOMPARE(buildProfile(u"minion-es")->id, u"minion-es"_s);
        QVERIFY(!buildProfile(u"nothing like it"));
        QCOMPARE(buildProfiles().size(), 3);
        for (const BuildProfile &p : buildProfiles()) {
            for (const ProfileStat &s : p.stats)
                QVERIFY2(s.pattern.isValid(), qPrintable(p.id + u": "_s + s.label));
        }
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
};

QTEST_GUILESS_MAIN(TestGearScore)
#include "tst_gearscore.moc"
