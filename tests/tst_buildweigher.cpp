// SPDX-License-Identifier: MIT
#include "app/activecharacter.h"
#include "app/buildweigher.h"
#include "fakepob.h"
#include "services/pobengine.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/></PathOfBuilding2>"_s;
const QString otherBuild =
    u"<PathOfBuilding2><Build level=\"80\" className=\"Warrior\"/></PathOfBuilding2>"_s;

QJsonObject life(double weight)
{
    return {{u"id"_s, u"explicit.stat_life"_s},
            {u"weight"_s, weight},
            {u"text"_s, u"+# to maximum Life"_s},
            {u"top"_s, 100}};
}

// Characters with the fake Path of Building, and the weigher over them.
struct Weighing
{
    QTemporaryDir dir;
    CharacterStore store{dir.path()};
    FakePob pob;
    PobEngine engine;
    ActiveCharacter active{&store, &engine};
    BuildWeigher weigher{&active, &engine};
    bool busy = false;

    Weighing()
    {
        engine.setInstall(pob.install());
        weigher.setBusy([this] { return busy; });
        pob.loads({{u"stats"_s, QJsonObject{{u"FullDPS"_s, 1000}, {u"TotalEHP"_s, 5000}}}});
        pob.weighs({{u"category"_s, u"armour.gloves"_s}, {u"mods"_s, QJsonArray{life(1)}}});
    }

    QString add(const QString &name, const QString &xml = build)
    {
        Character character;
        character.name = name;
        const QString id = store.add(character).value_or(QString());
        if (!xml.isEmpty())
            store.savePobBuild(id, xml);
        return id;
    }

    // Settles when it has stopped weighing, which is only after it began.
    bool settles(int ms = 20000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < ms) {
            QTest::qWait(50);
            if (!weigher.isWeighing() && (active.character().weighed || pob.weighings() > 0))
                return true;
        }
        return false;
    }

    // The groups it asks Path of Building about
    static constexpr int askedAbout = 9;
};

} // namespace

class TestBuildWeigher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void weighsOnceTheBuildIsLoaded()
    {
        Weighing w;
        const QString id = w.add(u"Torvald"_s);
        w.weigher.setSettings({});
        w.active.setId(id);
        // with nothing asked of Path of Building but the weighing
        QVERIFY(w.settles());
        const auto weighed = w.active.character().weighed;
        QVERIFY(weighed);
        QCOMPARE(w.pob.weighings(), Weighing::askedAbout);
        QVERIFY(weighed->covers(u"Boots"_s));
        QVERIFY(weighed->covers(u"Ring"_s));
        // Path of Building has no slot for jewels
        QVERIFY(!weighed->covers(u"Jewel"_s));
        QCOMPARE(weighed->pobVersion, u"0.1.0"_s);
        QVERIFY(!weighed->date.isEmpty());
        QCOMPARE(weighed->build, buildFingerprint(build));
        QCOMPARE(weighed->damageWeight, PobSettings().damageWeight);

        // and keeps it with the character
        CharacterStore again(w.dir.path());
        again.load();
        QVERIFY(again.find(id)->weighed);
    }

    void doesntAskWhenItsOff()
    {
        Weighing w;
        PobSettings settings;
        settings.enabled = false;
        w.weigher.setSettings(settings);
        w.active.setId(w.add(u"Torvald"_s));
        QTest::qWait(500);
        QCOMPARE(w.pob.runs(), 0);
        QVERIFY(!w.active.character().weighed);
        QVERIFY(!w.weigher.weighAgain());
    }

    void waitsForAnItemCheck()
    {
        Weighing w;
        w.busy = true;
        w.weigher.setSettings({});
        w.active.setId(w.add(u"Torvald"_s));
        QTest::qWait(1500);
        QCOMPARE(w.pob.weighings(), 0);
        QVERIFY(w.weigher.isWeighing());
        w.busy = false;
        QVERIFY(w.settles());
        QCOMPARE(w.pob.weighings(), Weighing::askedAbout);
    }

    void doesntWeighAgainForTheSameBuild()
    {
        Weighing w;
        w.weigher.setSettings({});
        w.active.setId(w.add(u"Torvald"_s));
        QVERIFY(w.settles());
        // not for the same build loaded again, or the gear changed
        w.active.reload();
        QTest::qWait(1000);
        QCOMPARE(w.pob.weighings(), Weighing::askedAbout);
        QVERIFY(!w.weigher.isWeighing());
    }

    void weighsAgainForAnotherBuild()
    {
        Weighing w;
        w.weigher.setSettings({});
        const QString id = w.add(u"Torvald"_s);
        w.active.setId(id);
        QVERIFY(w.settles());
        QVERIFY(w.store.savePobBuild(id, otherBuild));
        w.active.reload();
        QTRY_COMPARE_WITH_TIMEOUT(w.pob.weighings(), 2 * Weighing::askedAbout, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(w.active.character().weighed->build, buildFingerprint(otherBuild), 5000);
    }

    void weighsAgainForAnotherDamageBalance()
    {
        Weighing w;
        w.weigher.setSettings({});
        w.active.setId(w.add(u"Torvald"_s));
        QVERIFY(w.settles());
        PobSettings settings;
        settings.damageWeight = 0.9;
        w.weigher.setSettings(settings);
        QTRY_COMPARE_WITH_TIMEOUT(w.pob.weighings(), 2 * Weighing::askedAbout, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(w.active.character().weighed->damageWeight, 0.9, 5000);
    }

    void anotherBalanceMidWeighStartsOver()
    {
        Weighing w;
        // holds it after the first group
        bool hold = true;
        w.weigher.setBusy([&w, &hold] { return hold && w.pob.weighings() >= 1; });
        w.weigher.setSettings({});
        w.active.setId(w.add(u"Torvald"_s));
        QTRY_COMPARE_WITH_TIMEOUT(w.pob.weighings(), 1, 20000);
        QVERIFY(w.weigher.isWeighing());

        PobSettings settings;
        settings.damageWeight = 0.9;
        w.weigher.setSettings(settings);
        hold = false;
        // every group by the new balance, the one done already as well
        QTRY_COMPARE_WITH_TIMEOUT(w.pob.weighings(), 1 + Weighing::askedAbout, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!w.weigher.isWeighing(), 5000);
        QCOMPARE(w.active.character().weighed->damageWeight, 0.9);
        QTest::qWait(500);
        QCOMPARE(w.pob.weighings(), 1 + Weighing::askedAbout);
    }

    void turnedOffMidWeighStops()
    {
        Weighing w;
        bool hold = true;
        w.weigher.setBusy([&w, &hold] { return hold && w.pob.weighings() >= 1; });
        w.weigher.setSettings({});
        w.active.setId(w.add(u"Torvald"_s));
        QTRY_COMPARE_WITH_TIMEOUT(w.pob.weighings(), 1, 20000);

        PobSettings settings;
        settings.enabled = false;
        w.weigher.setSettings(settings);
        QVERIFY(!w.weigher.isWeighing());
        hold = false;
        QTest::qWait(1000);
        QCOMPARE(w.pob.weighings(), 1);
        QVERIFY(!w.active.character().weighed);
    }

    void weighsAgainWhenAsked()
    {
        Weighing w;
        w.weigher.setSettings({});
        w.active.setId(w.add(u"Torvald"_s));
        QVERIFY(w.settles());
        QVERIFY(w.weigher.weighAgain());
        QTRY_COMPARE_WITH_TIMEOUT(w.pob.weighings(), 2 * Weighing::askedAbout, 20000);
    }

    void aFailureKeepsWhatItHadAndDoesntLoop()
    {
        Weighing w;
        w.weigher.setSettings({});
        const QString id = w.add(u"Torvald"_s);
        w.active.setId(id);
        QVERIFY(w.settles());
        const WeighedProfile before = *w.active.character().weighed;

        // Path of Building can't weigh now, and the build is another
        QVERIFY(w.pob.weighs({{u"ok"_s, false}, {u"error"_s, u"It broke."_s}}));
        QVERIFY(w.store.savePobBuild(id, otherBuild));
        // which it says for each group
        for (int i = 0; i < Weighing::askedAbout; ++i)
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"couldn't weigh the mods"_s));
        w.active.reload();
        QTRY_COMPARE_WITH_TIMEOUT(w.pob.weighings(), 2 * Weighing::askedAbout, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!w.weigher.isWeighing(), 5000);
        QCOMPARE(w.active.character().weighed->build, before.build);
        // and isn't asked again until something changes
        w.active.reload();
        QTest::qWait(1000);
        QCOMPARE(w.pob.weighings(), 2 * Weighing::askedAbout);
    }

    void aWeighingWithNothingInItIsLeftOut()
    {
        Weighing w;
        QVERIFY(w.pob.weighs({{u"category"_s, u"armour.gloves"_s}, {u"mods"_s, QJsonArray{}}}));
        w.weigher.setSettings({});
        w.active.setId(w.add(u"Torvald"_s));
        QTRY_COMPARE_WITH_TIMEOUT(w.pob.weighings(), Weighing::askedAbout, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!w.weigher.isWeighing(), 5000);
        QVERIFY(!w.active.character().weighed);
    }

    void judgedByYourWeightsAloneItIsntWeighed()
    {
        Weighing w;
        w.weigher.setSettings({});
        const QString id = w.add(u"Torvald"_s);
        Character character = *w.store.find(id);
        character.weightsOnly = true;
        QVERIFY(w.store.save(id, character));
        w.active.setId(id);
        QTest::qWait(500);
        QCOMPARE(w.pob.runs(), 0);
        QVERIFY(!w.weigher.canWeigh());
        QVERIFY(!w.weigher.weighAgain());

        // and weighs once you stop
        QVERIFY(w.active.change([](Character &c) { c.weightsOnly = false; }));
        QVERIFY(w.weigher.canWeigh());
        QVERIFY(w.settles());
        QVERIFY(w.active.character().weighed);
    }

    void aStartThatIsGoneCantBeWeighed()
    {
        Weighing w;
        w.weigher.setSettings({});
        const QString id = w.add(u"Torvald"_s);
        Character character = *w.store.find(id);
        character.buildProfile = u"glass-cannon"_s;
        QVERIFY(w.store.save(id, character));
        w.active.setId(id);
        QTest::qWait(500);
        QCOMPARE(w.pob.runs(), 0);
        QVERIFY(!w.weigher.canWeigh());
    }

    void anotherCharacterMidWeighDropsTheFirst()
    {
        Weighing w;
        w.busy = true;
        w.weigher.setSettings({});
        const QString first = w.add(u"Torvald"_s);
        const QString second = w.add(u"Astrid"_s, otherBuild);
        w.active.setId(first);
        QTest::qWait(300);
        QVERIFY(w.weigher.isWeighing());
        w.active.setId(second);
        w.busy = false;
        QVERIFY(w.settles());
        QTRY_VERIFY_WITH_TIMEOUT(w.active.character().weighed.has_value(), 5000);
        QCOMPARE(w.active.character().weighed->build, buildFingerprint(otherBuild));
        CharacterStore again(w.dir.path());
        again.load();
        QVERIFY(!again.find(first)->weighed);
        QVERIFY(again.find(second)->weighed);
    }
};

QTEST_MAIN(TestBuildWeigher)
#include "tst_buildweigher.moc"
