// SPDX-License-Identifier: MIT
#include "app/itemjudge.h"
#include "core/parser.h"
#include "fakepob.h"
#include "itemtexts.h"
#include "services/pobengine.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

using namespace pou;

namespace {

using Pob = ItemCheck::Pob;

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/></PathOfBuilding2>"_s;

Item item(const QString &text)
{
    return parseItem(text).value();
}

const Item helmetNew = item(itemtexts::helmetNew);
const Item helmetOld = item(itemtexts::helmetOld);

Character wearing(const Equipped &gear)
{
    Character c;
    c.gear = gear;
    return c;
}

// One of the places Path of Building tried an item in.
struct Place
{
    QString slot;
    QString replacing;
    QJsonObject stats;
};

// Its answer for one item: the build's numbers as it is, and with the item in
// each place it was tried in.
QJsonObject tried(const QJsonObject &before, const QList<Place> &places)
{
    QJsonArray slots;
    for (const Place &p : places) {
        slots.append(QJsonObject{
            {u"slot"_s, p.slot},
            {u"label"_s, p.slot},
            {u"replacing"_s, p.replacing},
            {u"stats"_s, p.stats},
        });
    }
    return {{u"before"_s, before},
            {u"results"_s, QJsonArray{QJsonObject{{u"ok"_s, true}, {u"slots"_s, slots}}}}};
}

const QJsonObject asItIs{{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1000}};
// effective HP 10% up, which is a score of 4 by default
const QJsonObject tougher{{u"FullDPS"_s, 100}, {u"TotalEHP"_s, 1100}};

// The judge with Path of Building, which has the build.
struct WithPob
{
    FakePob pob;
    PobEngine engine;
    ItemJudge judge;

    WithPob()
    {
        engine.setInstall(pob.install());
        engine.setBuild(build);
        judge.setEngine(&engine);
        judge.setBuild(PobBuild());
    }
};

} // namespace

class TestItemJudge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void judgesForTheCharacter()
    {
        ItemJudge judge;
        judge.setCharacter(wearing({{u"Helmet"_s, helmetOld}}));
        QSignalSpy checked(&judge, &ItemJudge::checked);
        judge.check(helmetNew);
        QCOMPARE(checked.size(), 1);

        const ItemCheck check = checked.first().at(0).value<ItemCheck>();
        QCOMPARE(check.item.name, u"Doom Visage"_s);
        QCOMPARE(check.evaluation.comparison.targetSlot, u"Helmet"_s);
        QCOMPARE(check.evaluation.comparison.replacing, QStringList{u"Grim Crown"_s});
        QCOMPARE(check.pob, Pob::Unused);
        QCOMPARE(judge.current().item.name, u"Doom Visage"_s);
    }

    void yourPriorities()
    {
        const Item spirit =
            item(u"Item Class: Amulets\nRarity: Rare\nGale Charm\nSolar Amulet\n--------\n+30 to Spirit"_s);
        const Item life = item(
            u"Item Class: Amulets\nRarity: Rare\nDoom Charm\nGold Amulet\n--------\n+64 to maximum Life"_s);
        ItemJudge judge;
        judge.setCharacter(wearing({{u"Amulet"_s, spirit}}));
        judge.check(life);
        QVERIFY(!judge.current().evaluation.risks.isEmpty());

        judge.setPriorities({.spirit = false});
        judge.check(life);
        QVERIFY(judge.current().evaluation.risks.isEmpty());
    }

    void notInstalled()
    {
        PobEngine engine;
        ItemJudge judge;
        judge.setEngine(&engine);
        judge.setBuild(PobBuild());
        judge.check(helmetNew);
        QCOMPARE(judge.current().pob, Pob::Unused);
    }

    void noBuild()
    {
        WithPob p;
        p.judge.setBuild(std::nullopt);
        p.judge.check(helmetNew);
        QCOMPARE(p.judge.current().pob, Pob::NoBuild);
        QCOMPARE(p.pob.runs(), 0);
    }

    void nothingToTryOn()
    {
        WithPob p;
        p.judge.setCharacter(wearing({{u"Helmet"_s, helmetOld}}));
        p.judge.check(item(
            u"Item Class: Stackable Currency\nRarity: Currency\nDivine Orb\n--------\nStack Size: 1/20"_s));
        QCOMPARE(p.judge.current().pob, Pob::Unused);
        // what you wear
        p.judge.check(helmetOld);
        QCOMPARE(p.judge.current().evaluation.comparison.verdict, Verdict::Equipped);
        QCOMPARE(p.judge.current().pob, Pob::Unused);
        QCOMPARE(p.pob.runs(), 0);
    }

    void judgedByPathOfBuilding()
    {
        WithPob p;
        QVERIFY(p.pob.answers(tried(asItIs, {{u"Helmet"_s, u"Grim Crown, Visored Helm"_s, tougher}})));
        p.judge.setCharacter(wearing({{u"Helmet"_s, helmetOld}}));
        QSignalSpy checked(&p.judge, &ItemJudge::checked);
        p.judge.check(helmetNew);
        QCOMPARE(checked.size(), 1);
        QCOMPARE(p.judge.current().pob, Pob::Checking);
        // by the gear score until it answers
        QCOMPARE(p.judge.current().evaluation.mode, ScoreMode::GearScores);

        QVERIFY(checked.wait());
        // in each place it can go in
        QCOMPARE(p.pob.asked(), (QJsonArray{QJsonObject{{u"text"_s, helmetNew.raw}}}));
        const ItemCheck &check = p.judge.current();
        QCOMPARE(check.pob, Pob::Checked);
        QCOMPARE(check.pobError, QString());
        QCOMPARE(check.evaluation.mode, ScoreMode::PathOfBuilding);
        QCOMPARE(check.evaluation.comparison.verdict, Verdict::Upgrade);
        QCOMPARE(check.evaluation.comparison.delta, 4);
        QCOMPARE(check.evaluation.comparison.replacing, QStringList{u"Grim Crown"_s});
        QCOMPARE(check.evaluation.pob->placement.slot, u"Helmet"_s);
        QCOMPARE(checked.at(1).at(0).value<ItemCheck>().pob, Pob::Checked);
        QTRY_VERIFY(p.engine.findChildren<PobReply *>().isEmpty());
    }

    void yourPrioritiesForPathOfBuilding()
    {
        // short of Strength with the item, which you can say doesn't matter
        const QJsonObject before{{u"FullDPS"_s, 100}, {u"Str"_s, 70}, {u"ReqStr"_s, 60}};
        const QJsonObject after{{u"FullDPS"_s, 110}, {u"Str"_s, 50}, {u"ReqStr"_s, 60}};
        WithPob p;
        QVERIFY(p.pob.answers(tried(before, {{u"Helmet"_s, {}, after}})));
        QSignalSpy checked(&p.judge, &ItemJudge::checked);
        p.judge.check(helmetNew);
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.current().evaluation.comparison.verdict, Verdict::Risky);

        p.judge.setPriorities({.attributes = false});
        p.judge.check(helmetNew);
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.current().evaluation.comparison.verdict, Verdict::Upgrade);
    }

    void cantTryItOn()
    {
        WithPob p;
        QVERIFY(p.pob.answers(
            {{u"before"_s, asItIs},
             {u"results"_s,
              QJsonArray{QJsonObject{{u"ok"_s, false}, {u"error"_s, u"This item can't go in Helmet."_s}}}}}));
        QSignalSpy checked(&p.judge, &ItemJudge::checked);
        p.judge.check(helmetNew);
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.current().pob, Pob::Failed);
        QCOMPARE(p.judge.current().pobError, u"This item can't go in Helmet."_s);
        // still judged the other way
        QCOMPARE(p.judge.current().evaluation.mode, ScoreMode::GearScores);
    }

    void triedNowhere()
    {
        WithPob p;
        QVERIFY(p.pob.answers(tried(asItIs, {})));
        QSignalSpy checked(&p.judge, &ItemJudge::checked);
        p.judge.check(helmetNew);
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.current().pob, Pob::Failed);
        QCOMPARE(p.judge.current().pobError, u"Path of Building didn't say how the item does."_s);
    }

    void doesntStart()
    {
        WithPob p;
        p.engine.setInstall(p.pob.install({u"--fail"_s}));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"^The fake didn't start\\."_s));
        QSignalSpy checked(&p.judge, &ItemJudge::checked);
        p.judge.check(helmetNew);
        QVERIFY(checked.wait());
        QCOMPARE(p.judge.current().pob, Pob::Failed);
        QCOMPARE(p.judge.current().pobError, u"The fake didn't start."_s);
    }

    void latestItem()
    {
        WithPob p;
        QVERIFY(p.pob.answers(tried(asItIs, {{u"Helmet"_s, {}, tougher}})));
        QSignalSpy checked(&p.judge, &ItemJudge::checked);
        p.judge.check(helmetNew);
        p.judge.check(helmetOld);
        QVERIFY(checked.wait());
        // the answer for the first one isn't wanted
        QCOMPARE(checked.size(), 3);
        QCOMPARE(checked.at(2).at(0).value<ItemCheck>().item.name, u"Grim Crown"_s);
        QCOMPARE(p.judge.current().pob, Pob::Checked);
        QVERIFY(!checked.wait(200));
    }

    void goneWhileChecking()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        {
            ItemJudge judge;
            judge.setEngine(&engine);
            judge.setBuild(PobBuild());
            judge.check(helmetNew);
            QCOMPARE(engine.findChildren<PobReply *>().size(), 1);
        }
        QVERIFY(engine.findChildren<PobReply *>().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestItemJudge)
#include "tst_itemjudge.moc"
