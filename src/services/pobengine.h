// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobbuild.h"
#include "core/pobinstall.h"

#include <QByteArray>
#include <QDeadlineTimer>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <chrono>
#include <optional>

class QProcess;

namespace pou {

// A request to Path of Building. It belongs to the engine, and is yours to
// delete once it has finished, even in a slot connected to finished(), or
// before then when you no longer want the answer.
class PobReply : public QObject
{
    Q_OBJECT

public:
    bool isFinished() const { return m_finished; }
    // What went wrong, for people, or empty when nothing did.
    QString errorString() const { return m_errorString; }
    // Path of Building's answer when nothing went wrong, see pobbridge.lua.
    QJsonObject answer() const { return m_answer; }

Q_SIGNALS:
    // Always from the event loop, never from inside a call to the engine.
    void finished();

private:
    friend class PobEngine;

    explicit PobReply(QObject *parent);
    void succeed(const QJsonObject &answer);
    void fail(const QString &why);
    void finish();

    bool m_finished = false;
    QString m_errorString;
    QJsonObject m_answer;
};

// Path of Building for Path of Exile 2, run without its window to judge items
// for a build by its own calculations. It starts the first time it's asked
// something. When it can't, what it's asked fails with why until it's tried
// again a while later.
class PobEngine : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Unavailable, // there's none to run, see errorString()
        Stopped,     // and starts when it's asked something
        Starting,
        Ready,
        Failed, // couldn't start, see errorString()
    };
    Q_ENUM(State)

    struct Timeouts
    {
        // to start, which takes a second or so
        std::chrono::milliseconds start = std::chrono::seconds(30);
        // to load the build, which a heavy one takes a while to do
        std::chrono::milliseconds load = std::chrono::seconds(120);
        // to answer anything else, with more for each item
        std::chrono::milliseconds request = std::chrono::seconds(15);
        std::chrono::milliseconds perItem = std::chrono::seconds(1);
        // with nothing to do, until it's stopped to free its memory
        std::chrono::milliseconds idle = std::chrono::minutes(10);
        // after it couldn't start, until it's tried again
        std::chrono::milliseconds retry = std::chrono::seconds(30);
        // to stop on its own before it's killed
        std::chrono::milliseconds quit = std::chrono::seconds(2);
    };

    explicit PobEngine(QObject *parent = nullptr);
    ~PobEngine() override;

    void setTimeouts(const Timeouts &timeouts);

    // The Path of Building to run, see findPob(). When it's another one, the
    // one that's running is stopped.
    void setInstall(const PobInstall &install);
    // That there's none to run, with why in words for people. The one that's
    // running is stopped.
    void setUnavailable(const QString &why);
    // The build to judge items for, in Path of Building's XML, with the gear
    // you're wearing in place of its own.
    void setBuild(const QString &xml, const QList<PobItem> &wearing = {});

    // The build's numbers with each item, in the slot it names or each one it
    // can go in. See eval in pobbridge.lua for the answer.
    PobReply *evaluate(const QList<PobItem> &items);
    // Stops it, failing what it hasn't answered. It starts again the next time
    // it's asked something, even when it couldn't start before.
    void stop();

    State state() const { return m_state; }
    // The one it runs, when there's one.
    const std::optional<PobInstall> &install() const { return m_install; }
    // Why it can't run, when it's Unavailable or Failed.
    QString errorString() const { return m_errorString; }
    // Path of Building's, once it has started.
    QString version() const { return m_version; }

Q_SIGNALS:
    void stateChanged(pou::PobEngine::State state);
    // When it's given another to run, or none, or another reason there's
    // none.
    void installChanged();
    // Once it has the build and your gear on, each time it starts or is given
    // another. See summary in pobbridge.lua for what it says of it.
    void buildLoaded(const QJsonObject &summary);

private:
    // A request Path of Building hasn't answered yet.
    struct Sent
    {
        int id = 0;
        qsizetype items = 0;
        bool eval = false;        // from evaluate(), not the build or your gear
        bool load = false;        // the build itself, which has its own time
        QPointer<PobReply> reply; // for an eval, null once it's deleted
    };

    void start();
    void sendBuild();
    void send(QJsonObject request, PobReply *reply = nullptr);
    void readAnswers();
    void started(const QJsonObject &answer);
    void answered(const Sent &sent, const QJsonObject &answer);
    void halt();
    void giveUp(const QString &why);
    void failStart(const QString &why);
    void failAll(const QString &why);
    void retire();
    void warn(const QString &why) const;
    std::chrono::milliseconds timeToAnswer(const Sent &sent) const;
    void watch();
    void timedOut();
    void setState(State state);

    std::optional<PobInstall> m_install;
    QString m_xml;
    QList<PobItem> m_wearing;
    Timeouts m_timeouts;
    State m_state = State::Unavailable;
    QString m_errorString;
    QDeadlineTimer m_retry; // when it's Failed, until it's tried again
    QString m_version;
    QProcess *m_process = nullptr;
    QByteArray m_printed; // the end of what it has printed
    QList<Sent> m_sent;   // in the order they were sent
    int m_lastId = 0;
    int m_buildId = 0; // what's sent last for the build
    QTimer m_timer;    // see watch()
};

} // namespace pou
