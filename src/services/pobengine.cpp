// SPDX-License-Identifier: MIT
#include "services/pobengine.h"

#include "services/logging.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// What LuaJIT runs. It reads the bridge from stdin, after its length on a line
// of its own, as the Flatpak's sandbox can't see the app's files.
constexpr QStringView bootstrap =
    u"local n = tonumber(io.read('*l')); assert(loadstring(io.read(n), '=pobbridge.lua'))()";

QByteArray bridge()
{
    QFile file(u":/pobbridge.lua"_s);
    if (!file.open(QIODevice::ReadOnly))
        qCCritical(lcPob, "The bridge to Path of Building is missing from the app");
    return file.readAll();
}

QJsonArray toJson(const QList<PobItem> &items)
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

PobReply::PobReply(QObject *parent)
    : QObject(parent)
{}

void PobReply::succeed(const QJsonObject &answer)
{
    m_answer = answer;
    finish();
}

void PobReply::fail(const QString &why)
{
    m_errorString = why;
    finish();
}

void PobReply::finish()
{
    m_finished = true;
    // so whoever asked can connect to it first, and delete it or the engine
    // when it's done
    QMetaObject::invokeMethod(this, &PobReply::finished, Qt::QueuedConnection);
}

PobEngine::PobEngine(QObject *parent)
    : QObject(parent)
{}

PobEngine::~PobEngine()
{
    if (!m_process)
        return;
    // it stops once stdin is closed, or it's killed if that takes too long
    m_process->disconnect(this);
    m_process->closeWriteChannel();
    if (!m_process->waitForFinished(2000)) {
        m_process->kill();
        m_process->waitForFinished();
    }
}

void PobEngine::setInstall(const PobInstall &install)
{
    m_install = install;
}

void PobEngine::setBuild(const QString &xml, const QList<PobItem> &wearing)
{
    if (xml == m_xml && wearing == m_wearing)
        return;
    m_xml = xml;
    m_wearing = wearing;
    if (m_process && !m_xml.isEmpty())
        sendBuild();
}

PobReply *PobEngine::evaluate(const QList<PobItem> &items)
{
    auto *reply = new PobReply(this);
    if (!m_install) {
        reply->fail(u"There's no Path of Building to run."_s);
        return reply;
    }
    if (m_xml.isEmpty()) {
        reply->fail(u"There's no build to judge items with."_s);
        return reply;
    }
    if (!m_process)
        start();
    send({{u"cmd"_s, u"eval"_s}, {u"items"_s, toJson(items)}}, reply);
    return reply;
}

void PobEngine::start()
{
    qCInfo(lcPob, "Starting Path of Building in %s", qUtf8Printable(m_install->folder));
    m_process = new QProcess(this);
    m_process->setProgram(m_install->program);
    m_process->setArguments(m_install->arguments + QStringList{u"-e"_s, bootstrap.toString()});
    m_process->setWorkingDirectory(m_install->folder);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &PobEngine::readAnswers);
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        // what Path of Building prints, which is only for finding out why
        // something didn't work
        qCDebug(lcPob, "%s", m_process->readAllStandardError().trimmed().constData());
    });
    setState(State::Starting);
    m_process->start();

    static const QByteArray script = bridge();
    m_process->write(QByteArray::number(script.size()) + '\n' + script);
    sendBuild();
}

void PobEngine::sendBuild()
{
    send({{u"cmd"_s, u"load"_s}, {u"xml"_s, m_xml}});
    if (!m_wearing.isEmpty())
        send({{u"cmd"_s, u"equip"_s}, {u"items"_s, toJson(m_wearing)}});
}

void PobEngine::send(QJsonObject request, PobReply *reply)
{
    request.insert(u"id"_s, ++m_lastId);
    m_process->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    m_sent << Sent{m_lastId, reply};
}

void PobEngine::readAnswers()
{
    // The bridge's answers start with @@, so anything else that gets printed
    // can be told apart.
    while (m_process->canReadLine()) {
        const QByteArray line = m_process->readLine();
        if (line.startsWith("@@"))
            answered(QJsonDocument::fromJson(line.sliced(2)).object());
    }
}

void PobEngine::answered(const QJsonObject &answer)
{
    // the first says it has started
    if (m_state == State::Starting) {
        m_version = answer.value(u"version").toString();
        qCInfo(lcPob, "Path of Building %s is ready", qUtf8Printable(m_version));
        setState(State::Ready);
        return;
    }
    if (m_sent.isEmpty())
        return;
    const Sent sent = m_sent.takeFirst();
    const bool ok = answer.value(u"ok").toBool();
    const QString error = answer.value(u"error").toString();
    if (!sent.reply) {
        if (!ok)
            qCWarning(lcPob, "Path of Building couldn't load the build: %s", qUtf8Printable(error));
        // and when it's the gear you're wearing, for each item
        for (const QJsonValue result : answer.value(u"results").toArray()) {
            if (!result[u"ok"].toBool())
                qCWarning(lcPob, "Path of Building couldn't put on your gear: %s",
                          qUtf8Printable(result[u"error"].toString()));
        }
    } else if (ok) {
        sent.reply->succeed(answer);
    } else {
        sent.reply->fail(error);
    }
}

void PobEngine::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

} // namespace pou
