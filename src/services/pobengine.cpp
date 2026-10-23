// SPDX-License-Identifier: MIT
#include "services/pobengine.h"

#include "services/logging.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTimer>
#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// What LuaJIT runs. It reads the bridge from stdin, after its length on a line
// of its own, as the Flatpak's sandbox can't see the app's files.
constexpr QStringView bootstrap =
    u"local n = tonumber(io.read('*l')); assert(loadstring(io.read(n), '=pobbridge.lua'))()";

// How much of what Path of Building prints is kept, to log when it fails.
constexpr qsizetype maxPrinted = 4096;

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
    // What's still running is left to stop once stdin is closed, and killed
    // if that takes too long.
    if (m_process)
        retire();
    const QDeadlineTimer deadline(m_timeouts.quit);
    for (QProcess *process : findChildren<QProcess *>(Qt::FindDirectChildrenOnly)) {
        if (process->state() != QProcess::NotRunning &&
            !process->waitForFinished(int(deadline.remainingTime()))) {
            process->kill();
            process->waitForFinished();
        }
    }
}

void PobEngine::setTimeouts(const Timeouts &timeouts)
{
    m_timeouts = timeouts;
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
    // It's started if it isn't running, but not again straight after it
    // couldn't start.
    if (!m_process && (m_state != State::Failed || m_retry.hasExpired()))
        start();
    if (!m_process) {
        reply->fail(m_errorString);
        return reply;
    }
    send({{u"cmd"_s, u"eval"_s}, {u"items"_s, toJson(items)}}, reply);
    return reply;
}

void PobEngine::stop()
{
    if (m_process) {
        qCInfo(lcPob, "Stopping Path of Building");
        retire();
        failAll(u"Path of Building was stopped before it answered."_s);
    }
    m_errorString.clear();
    setState(State::Stopped);
}

void PobEngine::start()
{
    qCInfo(lcPob, "Starting Path of Building in %s, with %s", qUtf8Printable(m_install->folder),
           qUtf8Printable((QStringList{m_install->program} + m_install->arguments).join(u' ')));
    auto *process = new QProcess(this);
    m_process = process;
    m_printed.clear();
    m_errorString.clear();
    process->setProgram(m_install->program);
    process->setArguments(m_install->arguments + QStringList{u"-e"_s, bootstrap.toString()});
    process->setWorkingDirectory(m_install->folder);
    connect(process, &QProcess::readyReadStandardOutput, this, &PobEngine::readAnswers);
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        // what Path of Building prints, which is only for finding out why
        // something didn't work
        const QByteArray printed = process->readAllStandardError();
        qCDebug(lcPob, "%s", printed.trimmed().constData());
        m_printed.append(printed);
        if (m_printed.size() > maxPrinted)
            m_printed.remove(0, m_printed.size() - maxPrinted);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            failStart(u"Couldn't run Path of Building: %1"_s.arg(process->errorString()));
    });
    connect(process, &QProcess::finished, this, [this] {
        if (m_state == State::Starting)
            failStart(u"Path of Building stopped while starting."_s);
    });
    setState(State::Starting);
    process->start();
    // which fails straight away when there's no program
    if (m_process != process)
        return;

    static const QByteArray script = bridge();
    process->write(QByteArray::number(script.size()) + '\n' + script);
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
    // can be told apart. When an answer means it's given up on, the rest are
    // left unread.
    QProcess *process = m_process;
    while (m_process == process && process->canReadLine()) {
        const QByteArray line = process->readLine();
        if (line.startsWith("@@"))
            answered(QJsonDocument::fromJson(line.sliced(2)).object());
    }
}

void PobEngine::answered(const QJsonObject &answer)
{
    // the first says whether it has started
    if (m_state == State::Starting) {
        if (!answer.value(u"ready").toBool()) {
            failStart(answer.value(u"error").toString(u"Path of Building didn't start."_s));
            return;
        }
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

// Fails what it was asked with why it couldn't start, and leaves it a while
// before it's tried again.
void PobEngine::failStart(const QString &why)
{
    warn(why);
    m_errorString = why;
    m_retry.setRemainingTime(m_timeouts.retry);
    retire();
    failAll(why);
    setState(State::Failed);
}

void PobEngine::failAll(const QString &why)
{
    for (const Sent &sent : std::exchange(m_sent, {})) {
        if (sent.reply)
            sent.reply->fail(why);
    }
}

// Stops listening to the process and leaves it to stop once stdin is closed,
// or kills it if that takes too long.
void PobEngine::retire()
{
    QProcess *process = std::exchange(m_process, nullptr);
    process->disconnect(this);
    if (process->state() == QProcess::NotRunning) {
        process->deleteLater();
        return;
    }
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    process->closeWriteChannel();
    QTimer::singleShot(m_timeouts.quit, process, &QProcess::kill);
}

// With the end of what Path of Building printed, which often says more.
void PobEngine::warn(const QString &why) const
{
    const QByteArray printed = m_printed.trimmed();
    if (printed.isEmpty())
        qCWarning(lcPob, "%s", qUtf8Printable(why));
    else
        qCWarning(lcPob, "%s It printed:\n%s", qUtf8Printable(why), printed.constData());
}

void PobEngine::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

} // namespace pou
