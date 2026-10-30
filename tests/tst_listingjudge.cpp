// SPDX-License-Identifier: MIT
#include "app/listingjudge.h"
#include "core/parser.h"
#include "fakepob.h"
#include "itemtexts.h"
#include "services/pobengine.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <utility>

using namespace pou;

namespace {

using Pob = ItemCheck::Pob;
using Range = std::pair<qsizetype, qsizetype>;

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/></PathOfBuilding2>"_s;

Item item(const QString &text)
{
    return parseItem(text).value();
}

const Item helmetNew = item(itemtexts::helmetNew);
const Item helmetOld = item(itemtexts::helmetOld);
const Item divineOrb =
    item(u"Item Class: Stackable Currency\nRarity: Currency\nDivine Orb\n--------\nStack Size: 1/20"_s);

Item helmet(const QString &name, int life)
{
    return item(u"Item Class: Helmets\nRarity: Rare\n"_s + name +
                u"\nIron Hat\n--------\n+%1 to maximum Life"_s.arg(life));
}

Character wearing(const Equipped &gear)
{
    Character c;
    c.gear = gear;
    return c;
}

const Character wearingOld = wearing({{u"Helmet"_s, helmetOld}});

// What the checked signal said changed.
Range rangeOf(const QList<QVariant> &arguments)
{
    return {arguments.at(0).value<qsizetype>(), arguments.at(1).value<qsizetype>()};
}

const QJsonObject asItIs{{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1000}};
// effective HP 10% up, which is a score of 4 by default, or 10% down
const QJsonObject tougher{{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}};
const QJsonObject weaker{{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 900}};

// Path of Building's answer for an item it tried as your helmet.
QJsonObject asHelmet(const QJsonObject &stats)
{
    const QJsonObject slot{{u"slot"_s, u"Helmet"_s},
                           {u"label"_s, u"Helmet"_s},
                           {u"replacing"_s, u"Grim Crown, Visored Helm"_s},
                           {u"stats"_s, stats}};
    return {{u"ok"_s, true}, {u"slots"_s, QJsonArray{slot}}};
}

QJsonObject couldnt(const QString &why)
{
    return {{u"ok"_s, false}, {u"error"_s, why}};
}

// Its answer for the items it was asked about, in their order.
QJsonObject tried(const QJsonArray &results, const QJsonObject &before = asItIs)
{
    return {{u"before"_s, before}, {u"results"_s, results}};
}

// The judge with Path of Building, which has the build.
struct WithPob
{
    FakePob pob;
    PobEngine engine;
    ListingJudge judge;

    explicit WithPob(const QStringList &flags = {})
    {
        engine.setInstall(pob.install(flags));
        engine.setBuild(build);
        judge.setPob(&engine);
        judge.setCharacter(wearingOld, PobBuild());
    }
};

} // namespace

class TestListingJudge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void judgesWhatItsGiven()
    {
        ListingJudge judge;
        judge.setCharacter(wearingOld, std::nullopt);
        QSignalSpy checked(&judge, &ListingJudge::checked);
        judge.add({helmetNew, helmet(u"Grim Ward"_s, 10)});
        judge.add({helmet(u"Storm Ward"_s, 90)});
        // as they're added rather than changed
        QCOMPARE(checked.size(), 0);

        const QList<ItemCheck> &checks = judge.checks();
        QCOMPARE(checks.size(), 3);
        QCOMPARE(checks.at(0).item.name, u"Doom Visage"_s);
        QCOMPARE(checks.at(2).item.name, u"Storm Ward"_s);
        for (const ItemCheck &check : checks) {
            QCOMPARE(check.evaluation.comparison.replacing, QStringList{u"Grim Crown"_s});
            QCOMPARE(check.pob, Pob::Unused);
        }

        judge.clear();
        QVERIFY(judge.checks().isEmpty());
        QCOMPARE(checked.size(), 0);
    }

    void judgesAgainForAnotherCharacter()
    {
        ListingJudge judge;
        judge.add({helmetNew, helmetOld});
        QVERIFY(judge.checks().at(0).evaluation.comparison.replacing.isEmpty());

        QSignalSpy checked(&judge, &ListingJudge::checked);
        judge.setCharacter(wearingOld, std::nullopt);
        QCOMPARE(checked.size(), 1);
        QCOMPARE(rangeOf(checked.first()), Range(0, 1));
        QCOMPARE(judge.checks().at(0).evaluation.comparison.replacing, QStringList{u"Grim Crown"_s});
        QCOMPARE(judge.checks().at(1).evaluation.comparison.verdict, Verdict::Equipped);
    }

    void nothingToJudgeAgain()
    {
        // which isn't a change to tell the list about
        ListingJudge judge;
        QSignalSpy checked(&judge, &ListingJudge::checked);
        judge.setCharacter(wearingOld, std::nullopt);
        judge.setSettings({.enabled = false});
        judge.setPob(nullptr);
        QCOMPARE(checked.size(), 0);
    }

    void asksPathOfBuildingAFewAtATime()
    {
        WithPob p;
        QJsonArray better;
        for (int i = 0; i < 5; ++i)
            better << asHelmet(tougher);
        QVERIFY(p.pob.answers(tried(better)));
        QList<Item> helmets;
        for (int i = 1; i <= 7; ++i)
            helmets << helmet(u"Grim Ward %1"_s.arg(i), 10 * i);
        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        p.judge.add(helmets);
        for (const ItemCheck &check : p.judge.checks()) {
            QCOMPARE(check.pob, Pob::Checking);
            // by the gear score until it answers
            QCOMPARE(check.evaluation.mode, ScoreMode::GearScores);
        }

        QVERIFY(checked.wait());
        QCOMPARE(rangeOf(checked.at(0)), Range(0, 4));
        QCOMPARE(p.judge.checks().at(4).pob, Pob::Checked);
        QCOMPARE(p.judge.checks().at(5).pob, Pob::Checking);

        QVERIFY(checked.wait());
        QCOMPARE(rangeOf(checked.at(1)), Range(5, 6));
        QCOMPARE(p.pob.asked(), (QJsonArray{QJsonObject{{u"text"_s, helmets.at(5).raw}},
                                            QJsonObject{{u"text"_s, helmets.at(6).raw}}}));
        for (const ItemCheck &check : p.judge.checks()) {
            QCOMPARE(check.pob, Pob::Checked);
            QCOMPARE(check.evaluation.mode, ScoreMode::PathOfBuilding);
            QCOMPARE(check.evaluation.comparison.verdict, Verdict::Upgrade);
        }
        QTRY_VERIFY(p.engine.findChildren<PobReply *>().isEmpty());
    }

    void addedWhileItAsks()
    {
        WithPob p;
        QVERIFY(p.pob.answers(tried({asHelmet(tougher), asHelmet(tougher)})));
        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        p.judge.add({helmet(u"Grim Ward"_s, 10), helmet(u"Storm Ward"_s, 20)});
        // these wait for the answer about those
        p.judge.add({helmet(u"Dusk Ward"_s, 30), helmet(u"Ember Ward"_s, 40)});
        QCOMPARE(p.engine.findChildren<PobReply *>().size(), 1);

        QVERIFY(checked.wait());
        QCOMPARE(rangeOf(checked.at(0)), Range(0, 1));
        QVERIFY(checked.wait());
        QCOMPARE(rangeOf(checked.at(1)), Range(2, 3));
        QCOMPARE(p.pob.asked().at(0).toObject().value(u"text"_s).toString(), p.judge.checks().at(2).item.raw);
        for (const ItemCheck &check : p.judge.checks())
            QCOMPARE(check.pob, Pob::Checked);
    }

    void eachAnswerGoesWithItsItem()
    {
        WithPob p;
        QVERIFY(p.pob.answers(tried({asHelmet(tougher), asHelmet(weaker)})));
        // and none for the orb, which isn't gear
        p.judge.add({helmetNew, divineOrb, helmet(u"Grim Ward"_s, 10)});
        QCOMPARE(p.judge.checks().at(1).pob, Pob::Unused);

        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        QVERIFY(checked.wait());
        QCOMPARE(rangeOf(checked.first()), Range(0, 2));
        QCOMPARE(p.pob.asked().size(), 2);
        QCOMPARE(p.judge.checks().at(0).evaluation.comparison.verdict, Verdict::Upgrade);
        QCOMPARE(p.judge.checks().at(1).pob, Pob::Unused);
        QCOMPARE(p.judge.checks().at(2).evaluation.comparison.verdict, Verdict::Downgrade);
    }

    void whatItCouldntTry()
    {
        WithPob p;
        const QString why = u"There is nowhere in the build this item can go."_s;
        QVERIFY(p.pob.answers(tried({couldnt(why), asHelmet(tougher)})));
        p.judge.add({helmetNew, helmet(u"Grim Ward"_s, 10)});
        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.checks().at(0).pob, Pob::Failed);
        QCOMPARE(p.judge.checks().at(0).pobError, why);
        // which still has its gear score
        QCOMPARE(p.judge.checks().at(0).evaluation.mode, ScoreMode::GearScores);
        QCOMPARE(p.judge.checks().at(1).pob, Pob::Checked);
    }

    void whenPathOfBuildingFails()
    {
        WithPob p({u"--crash-on-eval"_s});
        p.judge.add({helmetNew, helmet(u"Grim Ward"_s, 10)});
        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        QVERIFY(checked.wait());
        for (const ItemCheck &check : p.judge.checks()) {
            QCOMPARE(check.pob, Pob::Failed);
            QCOMPARE(check.pobError, u"Path of Building stopped unexpectedly."_s);
        }
    }

    void calledOff()
    {
        WithPob p;
        QVERIFY(p.pob.answers(tried({asHelmet(tougher)})));
        p.judge.add({helmetNew});
        p.judge.clear();
        QVERIFY(p.engine.findChildren<PobReply *>().isEmpty());

        // so the answer to that one isn't taken for the next
        QVERIFY(p.pob.answers(tried({asHelmet(weaker), asHelmet(tougher)})));
        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        p.judge.add({helmet(u"Grim Ward"_s, 10), helmetNew});
        QVERIFY(checked.wait());
        QCOMPARE(checked.size(), 1);
        QCOMPARE(p.judge.checks().size(), 2);
        QCOMPARE(p.judge.checks().at(0).evaluation.comparison.verdict, Verdict::Downgrade);
        QCOMPARE(p.judge.checks().at(1).evaluation.comparison.verdict, Verdict::Upgrade);
    }

    void judgesAgainWhileItAsks()
    {
        WithPob p;
        QVERIFY(p.pob.answers(tried({asHelmet(tougher)})));
        p.judge.add({helmetNew});
        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        p.judge.setCharacter(wearingOld, PobBuild());
        QCOMPARE(checked.size(), 1);
        QCOMPARE(p.judge.checks().at(0).pob, Pob::Checking);

        // with only the answer to what it asked again
        QVERIFY(checked.wait());
        QCOMPARE(checked.size(), 2);
        QCOMPARE(p.judge.checks().at(0).pob, Pob::Checked);
    }

    void pathOfBuildingOffOrNoBuild()
    {
        WithPob p;
        p.judge.setSettings({.enabled = false});
        p.judge.add({helmetNew});
        QCOMPARE(p.judge.checks().at(0).pob, Pob::Unused);

        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        p.judge.setSettings({.enabled = false});
        QCOMPARE(checked.size(), 0);
        p.judge.setSettings({});
        QCOMPARE(checked.size(), 1);
        QCOMPARE(p.judge.checks().at(0).pob, Pob::Checking);

        p.judge.setCharacter(wearingOld, std::nullopt);
        QCOMPARE(p.judge.checks().at(0).pob, Pob::NoBuild);
        p.judge.setPob(nullptr);
        QCOMPARE(p.judge.checks().at(0).pob, Pob::Unused);
    }

    void turnedOffBeforeItsGiven()
    {
        WithPob p;
        p.judge.setSettings({.enabled = false});
        p.judge.setPob(&p.engine);
        p.judge.add({helmetNew});
        QCOMPARE(p.judge.checks().at(0).pob, Pob::Unused);
        QVERIFY(p.engine.findChildren<PobReply *>().isEmpty());
    }

    void yourSettings()
    {
        // short of Strength with the item, which you can say doesn't matter
        const QJsonObject before{{u"FullDPS"_s, 100}, {u"Str"_s, 70}, {u"ReqStr"_s, 60}};
        const QJsonObject after{{u"FullDPS"_s, 110}, {u"Str"_s, 50}, {u"ReqStr"_s, 60}};
        WithPob p;
        QVERIFY(p.pob.answers(tried({asHelmet(after)}, before)));
        p.judge.add({helmetNew});
        QSignalSpy checked(&p.judge, &ListingJudge::checked);
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.checks().at(0).evaluation.comparison.verdict, Verdict::Risky);

        p.judge.setSettings({.priorities = {.attributes = false}});
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.checks().at(0).evaluation.comparison.verdict, Verdict::Upgrade);

        // and with only damage counting, more effective HP is no better
        QVERIFY(p.pob.answers(tried({asHelmet(tougher)})));
        p.judge.setSettings({.damageWeight = 1});
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.checks().at(0).evaluation.comparison.verdict, Verdict::Same);
    }
};

QTEST_GUILESS_MAIN(TestListingJudge)
#include "tst_listingjudge.moc"
