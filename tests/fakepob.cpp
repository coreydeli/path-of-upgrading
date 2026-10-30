// SPDX-License-Identifier: MIT
//
// Stands in for LuaJIT running pobbridge.lua in Path of Building's folder, for
// testing the engine and what uses it. It answers as the bridge does without
// working anything out: its answers have the build and gear it has and the
// commands it has had, an eval's has the items it was sent, and a weigh's the
// slot and weights. Each has what's in a file in the folder, where a test can
// write what the bridge would have worked out: eval.json for an eval,
// weigh.json for a weigh and summary.json for loading the build or putting on
// gear. A build that isn't for Path of Exile 2 doesn't
// load. In the folder, it counts how many times it's run in a file called
// runs, and how many times it has stopped once stdin was closed in one called
// stops. It keeps the items it was last asked to judge in one called
// asked.json, and the slot and weights it was last asked to weigh in
// weighed.json.
//
// Its own arguments come before LuaJIT's, for going wrong:
//   --fail           it says it couldn't start
//   --crash          it stops before it has started
//   --silent         it never says it has started
//   --crash-on-eval  it stops when it's asked to judge items
//   --kill-on-eval   it's killed when it's asked to judge items
//   --garble         its answer to an eval isn't JSON
//   --hang-on-eval   it doesn't answer one
//   --hang-on-load   it doesn't answer loading the build, or anything after
//   --hang-on-equip  it doesn't answer putting on gear, or anything after
//   --slow-load      it takes half a second to load the build
//   --linger         it takes ten seconds to stop once stdin is closed
//   --once           only the first time it's run

#include <QByteArrayList>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

using namespace Qt::Literals::StringLiterals;

namespace {

QFile in;
QFile out;

[[noreturn]] void quit(const char *why)
{
    std::fprintf(stderr, "fakepob: %s\n", why);
    std::exit(2);
}

void answer(const QJsonObject &answer)
{
    // and something that isn't an answer, to be skipped
    out.write("Loading the fake\n@@" + QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n');
    out.flush();
}

// Adds one to the number in the file, and returns it.
int count(const QString &name)
{
    QFile file(name);
    const int n = (file.open(QIODevice::ReadOnly) ? file.readAll().toInt() : 0) + 1;
    file.close();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        quit("it can't count");
    file.write(QByteArray::number(n));
    return n;
}

} // namespace

int main(int argc, char *argv[])
{
    // LuaJIT's arguments come last: -e and the code that reads the bridge
    if (argc < 3 || std::strcmp(argv[argc - 2], "-e") != 0)
        quit("it wasn't given code to run");
    const QByteArrayList flags(argv + 1, argv + argc - 2);
    if (!in.open(stdin, QIODevice::ReadOnly) || !out.open(stdout, QIODevice::WriteOnly))
        quit("it can't read or write");
    const qint64 length = in.readLine().trimmed().toLongLong();
    // which can come in more than one go, as it does to LuaJIT
    QByteArray script;
    while (script.size() < length) {
        const QByteArray part = in.read(length - script.size());
        if (part.isEmpty())
            break;
        script += part;
    }
    if (script.size() != length || !script.startsWith("-- SPDX-License-Identifier: MIT\n--\n"
                                                      "-- Path of Upgrading's bridge to Path of Building."))
        quit("it wasn't sent the bridge");
    const int runs = count(u"runs"_s);
    const bool goWrong = !flags.contains("--once") || runs == 1;
    std::fprintf(stderr, "Uniques loaded\n");
    if (goWrong && flags.contains("--crash"))
        return 3;
    if (goWrong && flags.contains("--fail")) {
        answer({{u"ready"_s, false}, {u"error"_s, u"The fake didn't start."_s}});
        return 1;
    }
    if (goWrong && flags.contains("--silent")) {
        while (!in.readLine().isEmpty()) {
        }
        count(u"stops"_s);
        return 0;
    }

    answer({{u"ready"_s, true}, {u"version"_s, u"0.1.0"_s}});
    QString xml;
    QJsonValue wearing;
    QString noBuild = u"There's no build loaded."_s;
    QJsonArray seen;
    // until stdin is closed
    for (QByteArray line = in.readLine(); !line.isEmpty(); line = in.readLine()) {
        const QJsonObject request = QJsonDocument::fromJson(line).object();
        const QString command = request.value(u"cmd").toString();
        if (command == u"eval" && goWrong && flags.contains("--crash-on-eval")) {
            std::fprintf(stderr, "The fake crashed\n");
            return 4;
        }
        if (command == u"eval" && goWrong && flags.contains("--kill-on-eval")) {
            std::fprintf(stderr, "The fake was killed\n");
            std::raise(SIGKILL);
        }
        if (command == u"eval" && goWrong && flags.contains("--hang-on-eval"))
            continue;
        if (goWrong && ((command == u"load" && flags.contains("--hang-on-load")) ||
                        (command == u"equip" && flags.contains("--hang-on-equip")))) {
            // it answers nothing more, as the rest would come out of order
            while (!in.readLine().isEmpty()) {
            }
            break;
        }
        if (command == u"load" && goWrong && flags.contains("--slow-load"))
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (command == u"eval" && goWrong && flags.contains("--garble")) {
            out.write("@@{\"id\": " + QByteArray::number(request.value(u"id").toInt()) + ", \"ok\"\n");
            out.flush();
            continue;
        }
        seen.append(command);
        QJsonObject r{{u"id"_s, request.value(u"id")}, {u"ok"_s, true}};
        if (command == u"load") {
            xml = request.value(u"xml").toString();
            wearing = QJsonValue();
            noBuild =
                xml.startsWith(u"<PathOfBuilding2>") ? QString() : u"The fake couldn't load the build."_s;
        }
        if (!noBuild.isEmpty()) {
            r.insert(u"ok"_s, false);
            r.insert(u"error"_s, noBuild);
            answer(r);
            continue;
        }
        if (command == u"equip")
            wearing = request.value(u"items");
        if (command == u"eval") {
            if (QFile asked(u"asked.json"_s); asked.open(QIODevice::WriteOnly))
                asked.write(QJsonDocument(request.value(u"items").toArray()).toJson());
            r.insert(u"items"_s, request.value(u"items"));
        }
        if (command == u"weigh") {
            const QJsonObject weighed{{u"slot"_s, request.value(u"slot")},
                                      {u"weights"_s, request.value(u"weights")}};
            if (QFile file(u"weighed.json"_s); file.open(QIODevice::WriteOnly))
                file.write(QJsonDocument(weighed).toJson());
            for (auto it = weighed.constBegin(); it != weighed.constEnd(); ++it)
                r.insert(it.key(), it.value());
        }
        r.insert(u"xml"_s, xml);
        r.insert(u"wearing"_s, wearing);
        r.insert(u"seen"_s, seen);
        const bool asked = command == u"eval" || command == u"weigh";
        if (QFile worked(asked ? command + u".json"_s : u"summary.json"_s);
            worked.open(QIODevice::ReadOnly)) {
            const QJsonObject extra = QJsonDocument::fromJson(worked.readAll()).object();
            for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
                r.insert(it.key(), it.value());
        }
        answer(r);
    }
    if (flags.contains("--linger"))
        std::this_thread::sleep_for(std::chrono::seconds(10));
    count(u"stops"_s);
    return 0;
}
