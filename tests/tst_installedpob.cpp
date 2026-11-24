// SPDX-License-Identifier: MIT
//
// The engine with the Path of Building on this computer, which is normally
// the Flatpak. It's skipped when there's none.
#include "core/pobweighing.h"
#include "itemtexts.h"
#include "services/pobengine.h"
#include "services/pobfinder.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using namespace itemtexts;

namespace {

// A level 40 Sorceress with no passives, wearing only a helmet.
const QString build = uR"xml(<?xml version="1.0" encoding="UTF-8"?>
<PathOfBuilding2>
	<Build level="40" targetVersion="0_1" className="Sorceress" ascendClassName="None" mainSocketGroup="1" viewMode="ITEMS"/>
	<Tree activeSpec="1">
		<Spec treeVersion="0_5" classInternalId="7" ascendancyInternalId="" nodes=""/>
	</Tree>
	<Items activeItemSet="1">
		<Item id="1">
Rarity: RARE
Dusk Veil
Visored Helm
Armour: 100
Item Level: 50
Implicits: 0
+40 to maximum Life
		</Item>
		<ItemSet id="1">
			<Slot name="Helmet" itemId="1"/>
		</ItemSet>
	</Items>
</PathOfBuilding2>
)xml"_s;

// Path of Building takes a second or so to start, and longer the first time
// it's run.
bool finishes(PobReply *reply)
{
    QSignalSpy finished(reply, &PobReply::finished);
    return reply->isFinished() || finished.wait(60000);
}

} // namespace

class TestInstalledPob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QString why;
        const std::optional<PobInstall> install = findPob(pobPlaces(), {}, &why);
        if (!install)
            QSKIP(qPrintable(why));
        m_engine.setInstall(*install);
    }

    void judgesAnItem()
    {
        m_engine.setBuild(build);
        const std::unique_ptr<PobReply> reply(m_engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QVERIFY(!m_engine.version().isEmpty());

        const QJsonObject answer = reply->answer();
        const QJsonObject result = answer[u"results"][0].toObject();
        QVERIFY2(result[u"ok"].toBool(), qPrintable(result[u"error"].toString()));
        const QJsonArray tried = result[u"slots"].toArray();
        QCOMPARE(tried.size(), 1);
        QCOMPARE(tried[0][u"slot"].toString(), u"Helmet"_s);
        QCOMPARE(tried[0][u"replacing"].toString(), u"Dusk Veil, Visored Helm"_s);

        // it has more life and resistances than the build's helmet
        const QJsonObject before = answer[u"before"].toObject();
        const QJsonObject after = tried[0][u"stats"].toObject();
        QCOMPARE_GT(after[u"Life"].toDouble(), before[u"Life"].toDouble());
        QCOMPARE_GT(after[u"FireResist"].toDouble(), before[u"FireResist"].toDouble());
        QCOMPARE_GT(after[u"LightningResist"].toDouble(), before[u"LightningResist"].toDouble());
    }

    void wearing()
    {
        // so trying the same helmet changes nothing
        m_engine.setBuild(build, {{.text = helmetOld, .slot = u"Helmet"_s}});
        const std::unique_ptr<PobReply> reply(m_engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());

        const QJsonObject answer = reply->answer();
        const QJsonObject result = answer[u"results"][0].toObject();
        QVERIFY2(result[u"ok"].toBool(), qPrintable(result[u"error"].toString()));
        const QJsonArray tried = result[u"slots"].toArray();
        QCOMPARE(tried.size(), 1);
        QCOMPARE(tried[0][u"replacing"].toString(), u"Grim Crown, Visored Helm"_s);
        QCOMPARE(tried[0][u"stats"][u"Life"].toDouble(), answer[u"before"][u"Life"].toDouble());
    }

    void weighsMods()
    {
        m_engine.setBuild(build);
        const std::unique_ptr<PobReply> reply(
            m_engine.weigh(u"Helmet"_s, {{.stat = u"TotalEHP"_s, .weight = 1}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());

        const PobWeighing weighing = readPobWeighing(reply->answer());
        QCOMPARE(weighing.category, u"armour.helmet"_s);
        QCOMPARE_GT(weighing.min, 0.0);
        // life adds the most to a helmet for effective HP
        QVERIFY(!weighing.mods.isEmpty());
        QCOMPARE(weighing.mods.first().id, u"explicit.stat_3299347043"_s);
        // and says what it is and the most a helmet rolls of it
        const PobModAbout life = weighing.about.value(weighing.mods.first().id);
        QCOMPARE(statKeyOf(life.text), u"# to maximum Life"_s);
        QCOMPARE_GT(life.top, 0.0);
    }

    void importsACharacter()
    {
        // a made-up one: a flask in the first flask place, and a skill
        const QJsonObject flask{{u"inventoryId"_s, u"Flask"_s},
                                {u"x"_s, 0},
                                {u"name"_s, QString()},
                                {u"typeLine"_s, u"Lesser Life Flask"_s},
                                {u"frameType"_s, 0},
                                {u"ilvl"_s, 10},
                                {u"properties"_s, QJsonArray()},
                                {u"explicitMods"_s, QJsonArray()}};
        const QJsonObject level{{u"name"_s, u"Level"_s}, {u"values"_s, QJsonArray{QJsonArray{u"10"_s, 0}}}};
        const QJsonObject spark{{u"typeLine"_s, u"Spark"_s}, {u"properties"_s, QJsonArray{level}}};
        const QJsonObject character{{u"name"_s, u"FakeOne"_s},
                                    {u"class"_s, u"Sorceress"_s},
                                    {u"level"_s, 40},
                                    {u"equipment"_s, QJsonArray{flask}},
                                    {u"skills"_s, QJsonArray{spark}}};
        // with some of what the API leaves out left out
        const std::unique_ptr<PobReply> reply(m_engine.importCharacter(character));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        const QString xml = reply->answer()[u"xml"].toString();
        QVERIFY(xml.contains(u"<PathOfBuilding2>"));
        QVERIFY(xml.contains(u"className=\"Sorceress\""));
        // the flask went in a slot, the first flask place being Flask 1
        QVERIFY(xml.contains(u"Lesser Life Flask"));
        QVERIFY(
            QRegularExpression(
                uR"xml(<Slot [^>]*itemId="1"[^>]*name="Flask 1"|<Slot [^>]*name="Flask 1"[^>]*itemId="1")xml"_s)
                .match(xml)
                .hasMatch());
        QCOMPARE(reply->answer()[u"mainSkill"].toString(), u"Spark"_s);
    }

private:
    PobEngine m_engine;
};

QTEST_GUILESS_MAIN(TestInstalledPob)
#include "tst_installedpob.moc"
