// SPDX-License-Identifier: MIT
#include "itemtexts.h"
#include "services/pobengine.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using namespace itemtexts;

namespace {

using State = PobEngine::State;

// A stand-in for Path of Building in a temporary folder, see fakepob.cpp.
struct FakePob
{
    QTemporaryDir folder;

    PobInstall install() const { return {.folder = folder.path(), .program = QStringLiteral(FAKEPOB)}; }
    // how many times it has been started
    int runs() const
    {
        QFile file(folder.filePath(u"runs"_s));
        return file.open(QIODevice::ReadOnly) ? file.readAll().toInt() : 0;
    }
};

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Sorceress\"/></PathOfBuilding2>"_s;

// Waits for the reply to finish, if it hasn't.
bool finishes(PobReply *reply)
{
    QSignalSpy finished(reply, &PobReply::finished);
    return reply->isFinished() || finished.wait();
}

// The items an eval was sent, by what the fake says.
QJsonArray itemsSent(const QList<PobItem> &items)
{
    QJsonArray array;
    for (const PobItem &item : items) {
        QJsonObject object{{u"text"_s, item.text}};
        if (!item.slot.isEmpty())
            object.insert(u"slot"_s, item.slot);
        array.append(object);
    }
    return array;
}

} // namespace

class TestPobEngine : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void evaluates()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        QCOMPARE(engine.state(), State::Stopped);
        QList<State> states;
        connect(&engine, &PobEngine::stateChanged, this, [&states](State state) { states << state; });

        // text as the game copies it: the ring's with dashes that aren't
        // ASCII, the helmet's with Windows line ends
        const QList<PobItem> items{{.text = advancedRing}, {.text = helmetOld, .slot = u"Helmet"_s}};
        const std::unique_ptr<PobReply> reply(engine.evaluate(items));
        QCOMPARE(engine.state(), State::Starting);
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QCOMPARE(engine.state(), State::Ready);
        QCOMPARE(states, (QList<State>{State::Starting, State::Ready}));
        QCOMPARE(engine.version(), u"0.1.0"_s);
        QCOMPARE(pob.runs(), 1);

        const QJsonObject answer = reply->answer();
        QCOMPARE(answer.value(u"items").toArray(), itemsSent(items));
        QCOMPARE(answer.value(u"xml").toString(), build);
        QCOMPARE(answer.value(u"seen").toArray(), (QJsonArray{u"load"_s, u"eval"_s}));

        // and once it's running
        const std::unique_ptr<PobReply> again(engine.evaluate({{.text = twoHandMace}}));
        QVERIFY(finishes(again.get()));
        QCOMPARE(again->answer().value(u"seen").toArray(), (QJsonArray{u"load"_s, u"eval"_s, u"eval"_s}));
        QCOMPARE(pob.runs(), 1);
    }

    void wearing()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        const QList<PobItem> gear{{.text = helmetOld, .slot = u"Helmet"_s},
                                  {.text = advancedRing, .slot = u"Ring 2"_s}};
        engine.setBuild(build, gear);
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetNew}}));
        QVERIFY(finishes(reply.get()));
        const QJsonObject answer = reply->answer();
        QCOMPARE(answer.value(u"seen").toArray(), (QJsonArray{u"load"_s, u"equip"_s, u"eval"_s}));
        QCOMPARE(answer.value(u"wearing").toArray(), itemsSent(gear));
    }

    void changesBuild()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));

        // loaded again while it's running, but not when it's the same
        const QString other =
            u"<PathOfBuilding2><Build level=\"91\" className=\"Witch\"/></PathOfBuilding2>"_s;
        const QList<PobItem> gear{{.text = helmetOld, .slot = u"Helmet"_s}};
        engine.setBuild(other, gear);
        engine.setBuild(other, gear);
        reply.reset(engine.evaluate({{.text = advancedRing}}));
        QVERIFY(finishes(reply.get()));
        const QJsonObject answer = reply->answer();
        QCOMPARE(answer.value(u"xml").toString(), other);
        QCOMPARE(answer.value(u"wearing").toArray(), itemsSent(gear));
        QCOMPARE(answer.value(u"seen").toArray(),
                 (QJsonArray{u"load"_s, u"eval"_s, u"load"_s, u"equip"_s, u"eval"_s}));
        QCOMPARE(pob.runs(), 1);
    }

    void nothingToRun()
    {
        PobEngine engine;
        engine.setBuild(build);
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(reply->isFinished());
        QCOMPARE(reply->errorString(), u"There's no Path of Building to run."_s);
        QVERIFY(finishes(reply.get()));
        QCOMPARE(engine.state(), State::Stopped);
    }

    void noBuild()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QCOMPARE(reply->errorString(), u"There's no build to judge items with."_s);
        QVERIFY(finishes(reply.get()));
        // it isn't started for nothing
        QCOMPARE(engine.state(), State::Stopped);
        QCOMPARE(pob.runs(), 0);
    }

    void inOrder()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        const QList<QList<PobItem>> asked{
            {{.text = helmetOld}}, {{.text = advancedRing}}, {{.text = twoHandMace}}};
        std::vector<std::unique_ptr<PobReply>> replies;
        for (const QList<PobItem> &items : asked)
            replies.emplace_back(engine.evaluate(items));
        for (qsizetype i = 0; i < asked.size(); ++i) {
            QVERIFY(finishes(replies[i].get()));
            QCOMPARE(replies[i]->answer().value(u"items").toArray(), itemsSent(asked[i]));
        }
    }

    void deletedEarly()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        delete engine.evaluate({{.text = helmetOld}});
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = advancedRing}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->answer().value(u"items").toArray(), itemsSent({{.text = advancedRing}}));
    }
};

QTEST_GUILESS_MAIN(TestPobEngine)
#include "tst_pobengine.moc"
