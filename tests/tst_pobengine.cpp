// SPDX-License-Identifier: MIT
#include "itemtexts.h"
#include "services/pobengine.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <chrono>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;
using namespace itemtexts;

namespace {

using State = PobEngine::State;

// A stand-in for Path of Building in a temporary folder, see fakepob.cpp.
struct FakePob
{
    QTemporaryDir folder;

    // with the fake's arguments for going wrong
    PobInstall install(const QStringList &flags = {}) const
    {
        return {.folder = folder.path(), .program = QStringLiteral(FAKEPOB), .arguments = flags};
    }
    // how many times it has been started
    int runs() const { return count(u"runs"_s); }
    // and has stopped on its own
    int stops() const { return count(u"stops"_s); }

    int count(const QString &name) const
    {
        QFile file(folder.filePath(name));
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

    void notLoaded()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        // for Path of Exile 1
        engine.setBuild(u"<PathOfBuilding><Build level=\"90\"/></PathOfBuilding>"_s);
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), u"The fake couldn't load the build."_s);
        QVERIFY(reply->answer().isEmpty());
        QCOMPARE(engine.state(), State::Ready);
    }

    void nothingToRun()
    {
        PobEngine engine;
        QCOMPARE(engine.state(), State::Unavailable);
        engine.setBuild(build);
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(reply->isFinished());
        QCOMPARE(reply->errorString(), u"There's no Path of Building to run."_s);
        QVERIFY(finishes(reply.get()));
        QCOMPARE(engine.state(), State::Unavailable);
        QCOMPARE(engine.errorString(), u"There's no Path of Building to run."_s);
    }

    void unavailable()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));

        // as when it's uninstalled while it's running
        engine.setUnavailable(u"Path of Building isn't installed."_s);
        QCOMPARE(engine.state(), State::Unavailable);
        QCOMPARE(engine.errorString(), u"Path of Building isn't installed."_s);
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), u"Path of Building was stopped before it answered."_s);
        QTRY_COMPARE(pob.stops(), 1);

        reply.reset(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(reply->isFinished());
        QCOMPARE(reply->errorString(), u"Path of Building isn't installed."_s);
        // which stopping it doesn't change
        engine.stop();
        QCOMPARE(engine.state(), State::Unavailable);
        QCOMPARE(engine.errorString(), u"Path of Building isn't installed."_s);

        // until there's one again
        engine.setInstall(pob.install());
        QCOMPARE(engine.state(), State::Stopped);
        QCOMPARE(engine.errorString(), QString());
        QCOMPARE(pob.runs(), 1);
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

    void stops()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));

        // with something it hasn't answered
        reply.reset(engine.evaluate({{.text = advancedRing}}));
        engine.stop();
        QCOMPARE(engine.state(), State::Stopped);
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), u"Path of Building was stopped before it answered."_s);
        QTRY_COMPARE(pob.stops(), 1);

        // and it starts again when it's asked something
        reply.reset(engine.evaluate({{.text = advancedRing}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QCOMPARE(pob.runs(), 2);
    }

    void newInstall()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install());
        engine.setBuild(build);
        std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));

        // the same one again
        engine.setInstall(pob.install());
        QCOMPARE(engine.state(), State::Ready);

        // and then another while it's asked something
        FakePob other;
        reply.reset(engine.evaluate({{.text = advancedRing}}));
        engine.setInstall(other.install());
        QCOMPARE(engine.state(), State::Stopped);
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), u"Path of Building was stopped before it answered."_s);
        QTRY_COMPARE(pob.stops(), 1);

        reply.reset(engine.evaluate({{.text = advancedRing}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QCOMPARE(pob.runs(), 1);
        QCOMPARE(other.runs(), 1);
    }

    void startFails()
    {
        FakePob pob;
        PobEngine engine;
        engine.setTimeouts({.retry = 300ms});
        engine.setInstall(pob.install({u"--fail"_s, u"--once"_s}));
        engine.setBuild(build);
        // with what it printed when that was read in time
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"^The fake didn't start\\."_s));

        // what it was asked while it was starting
        const std::unique_ptr<PobReply> first(engine.evaluate({{.text = helmetOld}}));
        const std::unique_ptr<PobReply> second(engine.evaluate({{.text = advancedRing}}));
        QVERIFY(finishes(first.get()));
        QVERIFY(finishes(second.get()));
        QCOMPARE(first->errorString(), u"The fake didn't start."_s);
        QCOMPARE(second->errorString(), u"The fake didn't start."_s);
        QCOMPARE(engine.state(), State::Failed);
        QCOMPARE(engine.errorString(), u"The fake didn't start."_s);

        // it isn't tried again straight away
        std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(reply->isFinished());
        QCOMPARE(reply->errorString(), u"The fake didn't start."_s);
        QCOMPARE(pob.runs(), 1);

        // but is after a while
        QTest::qWait(350);
        reply.reset(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QCOMPARE(engine.state(), State::Ready);
        QCOMPARE(engine.errorString(), QString());
        QCOMPARE(pob.runs(), 2);
    }

    void fixedInstall()
    {
        FakePob broken;
        PobEngine engine;
        engine.setInstall(broken.install({u"--fail"_s}));
        engine.setBuild(build);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"^The fake didn't start\\."_s));
        std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(engine.state(), State::Failed);

        // which is started without waiting to try again
        FakePob pob;
        engine.setInstall(pob.install());
        QCOMPARE(engine.state(), State::Stopped);
        QCOMPARE(engine.errorString(), QString());
        reply.reset(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QCOMPARE(pob.runs(), 1);
    }

    void crashesStarting()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install({u"--crash"_s}));
        engine.setBuild(build);
        QTest::ignoreMessage(QtWarningMsg,
                             "Path of Building stopped while starting. It printed:\nUniques loaded");
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), u"Path of Building stopped while starting."_s);
        QCOMPARE(engine.state(), State::Failed);
    }

    void crashes()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install({u"--crash-on-eval"_s, u"--once"_s}));
        engine.setBuild(build);
        QTest::ignoreMessage(
            QtWarningMsg,
            "Path of Building stopped unexpectedly. It printed:\nUniques loaded\nThe fake crashed");
        std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), u"Path of Building stopped unexpectedly."_s);
        QCOMPARE(engine.state(), State::Stopped);

        // and it starts again when it's asked something
        reply.reset(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QCOMPARE(engine.state(), State::Ready);
        QCOMPARE(pob.runs(), 2);
    }

    void garbled()
    {
        FakePob pob;
        PobEngine engine;
        engine.setInstall(pob.install({u"--garble"_s, u"--once"_s}));
        engine.setBuild(build);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(u"^Path of Building's answer couldn't be read\\."_s));
        std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), u"Path of Building's answer couldn't be read."_s);
        QCOMPARE(engine.state(), State::Stopped);
        QTRY_COMPARE(pob.stops(), 1);

        reply.reset(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QCOMPARE(pob.runs(), 2);
    }

    void cantRun()
    {
        FakePob pob;
        PobEngine engine;
        // with no LuaJIT where it's meant to be
        engine.setInstall({.folder = pob.folder.path(), .program = pob.folder.filePath(u"luajit"_s)});
        engine.setBuild(build);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"^Couldn't run Path of Building: "_s));
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(finishes(reply.get()));
        QVERIFY2(reply->errorString().startsWith(u"Couldn't run Path of Building: "),
                 qPrintable(reply->errorString()));
        QCOMPARE(engine.state(), State::Failed);
    }

    void noProgram()
    {
        FakePob pob;
        PobEngine engine;
        // which fails as it's started, before evaluate() returns
        engine.setInstall({.folder = pob.folder.path()});
        engine.setBuild(build);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"^Couldn't run Path of Building: "_s));
        const std::unique_ptr<PobReply> reply(engine.evaluate({{.text = helmetOld}}));
        QVERIFY(reply->isFinished());
        QVERIFY2(reply->errorString().startsWith(u"Couldn't run Path of Building: "),
                 qPrintable(reply->errorString()));
        QCOMPARE(engine.state(), State::Failed);
        QVERIFY(finishes(reply.get()));
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
