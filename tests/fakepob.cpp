// SPDX-License-Identifier: MIT
//
// Stands in for LuaJIT running pobbridge.lua in Path of Building's folder, for
// testing the engine. It answers as the bridge does without working anything
// out: an eval's answer has the items it was sent, the build and gear it has
// and the commands it has had. It counts how many times it's run in a file
// called runs in the folder.

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

void countRun()
{
    QFile file(u"runs"_s);
    const int runs = file.open(QIODevice::ReadOnly) ? file.readAll().toInt() : 0;
    file.close();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        quit("it can't count its runs");
    file.write(QByteArray::number(runs + 1));
}

} // namespace

int main(int argc, char *argv[])
{
    // LuaJIT's arguments come last: -e and the code that reads the bridge
    if (argc < 3 || std::strcmp(argv[argc - 2], "-e") != 0)
        quit("it wasn't given code to run");
    if (!in.open(stdin, QIODevice::ReadOnly) || !out.open(stdout, QIODevice::WriteOnly))
        quit("it can't read or write");
    const qint64 length = in.readLine().trimmed().toLongLong();
    const QByteArray script = in.read(length);
    if (script.size() != length || !script.startsWith("-- SPDX-License-Identifier: MIT\n--\n"
                                                      "-- Path of Upgrading's bridge to Path of Building."))
        quit("it wasn't sent the bridge");
    countRun();
    std::fprintf(stderr, "Uniques loaded\n");

    answer({{u"ready"_s, true}, {u"version"_s, u"0.1.0"_s}});
    QString xml;
    QJsonValue wearing;
    QJsonArray seen;
    // until stdin is closed
    for (QByteArray line = in.readLine(); !line.isEmpty(); line = in.readLine()) {
        const QJsonObject request = QJsonDocument::fromJson(line).object();
        const QString command = request.value(u"cmd").toString();
        seen.append(command);
        QJsonObject r{{u"id"_s, request.value(u"id")}, {u"ok"_s, true}};
        if (command == u"load") {
            xml = request.value(u"xml").toString();
            wearing = QJsonValue();
        } else if (command == u"equip") {
            wearing = request.value(u"items");
        } else if (command == u"eval") {
            r.insert(u"items"_s, request.value(u"items"));
            r.insert(u"xml"_s, xml);
            r.insert(u"wearing"_s, wearing);
            r.insert(u"seen"_s, seen);
        }
        answer(r);
    }
    return 0;
}
