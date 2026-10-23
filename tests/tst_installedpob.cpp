// SPDX-License-Identifier: MIT
//
// The engine with the Path of Building on this computer, which is normally
// the Flatpak. It's skipped when there's none.
#include "itemtexts.h"
#include "services/pobengine.h"

#include <QJsonArray>
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

private:
    PobEngine m_engine;
};

QTEST_GUILESS_MAIN(TestInstalledPob)
#include "tst_installedpob.moc"
