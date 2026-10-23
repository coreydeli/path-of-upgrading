// SPDX-License-Identifier: MIT
#pragma once

#include "core/pobinstall.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <optional>

class QProcess;

namespace pou {

// An item for Path of Building: its text as the game copies it, and the slot
// to try it in by Path of Building's name for it, or none to try it in each
// one it can go in.
struct PobItem
{
    QString text;
    QString slot;
};

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
// something.
class PobEngine : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Stopped, // and starts when it's asked something
        Starting,
        Ready,
    };
    Q_ENUM(State)

    explicit PobEngine(QObject *parent = nullptr);
    ~PobEngine() override;

    // The Path of Building to run, see findPob().
    void setInstall(const PobInstall &install);
    // The build to judge items for, in Path of Building's XML, with the gear
    // you're wearing in place of its own.
    void setBuild(const QString &xml, const QList<PobItem> &wearing = {});

    // The build's numbers with each item, in the slot it names or each one it
    // can go in. See eval in pobbridge.lua for the answer.
    PobReply *evaluate(const QList<PobItem> &items);

    State state() const { return m_state; }
    // Path of Building's, once it has started.
    QString version() const { return m_version; }

Q_SIGNALS:
    void stateChanged(pou::PobEngine::State state);

private:
    // A request Path of Building hasn't answered yet.
    struct Sent
    {
        int id = 0;
        QPointer<PobReply> reply; // none for the build, and null once it's deleted
    };

    void start();
    void sendBuild();
    void send(QJsonObject request, PobReply *reply = nullptr);
    void readAnswers();
    void answered(const QJsonObject &answer);
    void setState(State state);

    std::optional<PobInstall> m_install;
    QString m_xml;
    QList<PobItem> m_wearing;
    State m_state = State::Stopped;
    QString m_version;
    QProcess *m_process = nullptr;
    QList<Sent> m_sent; // in the order they were sent
    int m_lastId = 0;
};

} // namespace pou
