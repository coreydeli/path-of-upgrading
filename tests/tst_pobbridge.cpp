// SPDX-License-Identifier: MIT
//
// The bridge itself, run by the LuaJIT on this computer in a stand-in for
// Path of Building's folder. It's skipped when LuaJIT isn't installed.
#include "services/pobengine.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Sorceress\"/></PathOfBuilding2>"_s;

// Loading the way newer LuaJIT does, which only skips a first line that
// starts with "#!".
const QString newerLuajit = uR"lua(
local loadstringAsIs = loadstring
function loadstring(text, name)
    if text:find("^#[^!]") then
        return nil, (name or "?"):gsub("^[@=]", "") .. ":1: unexpected symbol near #"
    end
    return loadstringAsIs(text, name)
end
function loadfile(path)
    local file, failure = io.open(path, "rb")
    if not file then
        return nil, failure
    end
    local text = file:read("*a")
    file:close()
    return loadstring(text, "@" .. path)
end
function dofile(path)
    return assert(loadfile(path))()
end
)lua"_s;

// Both start with a "#@" line, like Path of Building's own.
const QString wrapper = uR"lua(#@
-- the stand-in
dofile("Launch.lua")
)lua"_s;

const QString launch = uR"lua(#@ SimpleGraphic
launch = {versionNumber = "1.2.3", main = {mode = "LIST"}}
main = {popups = {}}
build = {
    mainSocketGroup = 1,
    skillsTab = {socketGroupList = {{displayLabel = "Spark"}}},
    itemsTab = {},
    calcsTab = {mainOutput = {Life = 1200}},
}

function build.itemsTab:UpdateSockets() end

function build.calcsTab:GetMiscCalculator()
    return function() return self.mainOutput end, self.mainOutput
end

function loadBuildFromXML()
    launch.main.mode = "BUILD"
end
)lua"_s;

// Only as much of dkjson as the bridge needs here: its answers, with plain
// text only, and the id and command of what it is asked.
const QString dkjson = uR"lua(
local json = {}

function json.encode(value)
    if type(value) == "string" then
        return ("%q"):format(value)
    elseif type(value) ~= "table" then
        return tostring(value)
    end
    local parts = {}
    if #value > 0 then
        for _, v in ipairs(value) do
            table.insert(parts, json.encode(v))
        end
        return "[" .. table.concat(parts, ",") .. "]"
    end
    for k, v in pairs(value) do
        table.insert(parts, json.encode(tostring(k)) .. ":" .. json.encode(v))
    end
    return "{" .. table.concat(parts, ",") .. "}"
end

function json.decode(line)
    return {id = tonumber(line:match([["id":(%d+)]])), cmd = line:match([["cmd":"(%a+)"]])}
end

return json
)lua"_s;

bool finishes(PobReply *reply)
{
    QSignalSpy finished(reply, &PobReply::finished);
    return reply->isFinished() || finished.wait(10000);
}

} // namespace

class TestPobBridge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_luajit = QStandardPaths::findExecutable(u"luajit"_s);
        if (m_luajit.isEmpty())
            QSKIP("LuaJIT isn't installed");
    }

    void init()
    {
        QTest::failOnWarning();
        QVERIFY(m_folder.isValid());
        QVERIFY(QDir(m_folder.path()).mkpath(u"lua"_s));
        QVERIFY(write(u"HeadlessWrapper.lua"_s, wrapper));
        QVERIFY(write(u"Launch.lua"_s, launch));
        QVERIFY(write(u"lua/dkjson.lua"_s, dkjson));
    }

    void starts_data() { luajits(); }
    void starts()
    {
        QFETCH(QStringList, arguments);
        PobEngine engine;
        engine.setInstall({.folder = m_folder.path(), .program = m_luajit, .arguments = arguments});
        QSignalSpy loaded(&engine, &PobEngine::buildLoaded);
        engine.setBuild(build);

        const std::unique_ptr<PobReply> reply(engine.evaluate({}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), QString());
        QCOMPARE(engine.version(), u"1.2.3"_s);
        QCOMPARE(reply->answer()[u"before"][u"Life"].toDouble(), 1200.0);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded[0][0].toJsonObject()[u"mainSkill"].toString(), u"Spark"_s);
    }

    // on the line it's on, though the first one is left out
    void saysWhereItFailed_data() { luajits(); }
    void saysWhereItFailed()
    {
        QFETCH(QStringList, arguments);
        QVERIFY(write(u"HeadlessWrapper.lua"_s, u"#@\n-- the stand-in\nerror(\"it broke\")\n"_s));
        PobEngine engine;
        engine.setInstall({.folder = m_folder.path(), .program = m_luajit, .arguments = arguments});
        engine.setBuild(build);

        const QString why = u"Path of Building didn't start: HeadlessWrapper.lua:3: it broke"_s;
        QTest::ignoreMessage(QtWarningMsg, qPrintable(why));
        const std::unique_ptr<PobReply> reply(engine.evaluate({}));
        QVERIFY(finishes(reply.get()));
        QCOMPARE(reply->errorString(), why);
    }

private:
    static void luajits()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("this one") << QStringList();
        QTest::newRow("newer") << QStringList{u"-e"_s, newerLuajit};
    }

    bool write(const QString &name, const QString &text) const
    {
        QFile file(m_folder.filePath(name));
        return file.open(QIODevice::WriteOnly) && file.write(text.toUtf8()) >= 0;
    }

    QString m_luajit;
    QTemporaryDir m_folder;
};

QTEST_GUILESS_MAIN(TestPobBridge)
#include "tst_pobbridge.moc"
