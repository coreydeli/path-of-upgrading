// SPDX-License-Identifier: MIT
#include "platform/restart.h"

#include <QProcess>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;
using namespace pou;

namespace {

// The old copy of the app, which stays until the test ends it.
class Lingering
{
public:
    Lingering()
    {
        m_process.start(u"sleep"_s, {u"30"_s});
        m_process.waitForStarted();
    }
    ~Lingering() { end(); }

    qint64 pid() const { return m_process.processId(); }
    void end()
    {
        m_process.kill();
        m_process.waitForFinished();
    }

private:
    QProcess m_process;
};

// Runs the command as the restart does, starting echo in place of the app.
void start(QProcess &restart, qint64 pid, std::chrono::seconds limit)
{
    const RestartCommand command =
        restartCommand(pid, u"echo"_s, {u"started"_s, u"with spaces"_s, u"$HOME"_s}, limit);
    restart.start(command.program, command.arguments);
    QVERIFY(restart.waitForStarted());
}

} // namespace

class TestRestart : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsOnceTheOldOneHasGone()
    {
        Lingering old;
        QProcess restart;
        start(restart, old.pid(), 60s);
        QVERIFY(!restart.waitForFinished(500));
        old.end();
        QVERIFY(restart.waitForFinished(5000));
        QCOMPARE(restart.readAllStandardOutput(), "started with spaces $HOME\n");
    }

    void startsAnywayAfterTheLimit()
    {
        Lingering old;
        QProcess restart;
        start(restart, old.pid(), 1s);
        QVERIFY(restart.waitForFinished(5000));
        QCOMPARE(restart.readAllStandardOutput(), "started with spaces $HOME\n");
    }
};

QTEST_GUILESS_MAIN(TestRestart)
#include "tst_restart.moc"
