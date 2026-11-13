// SPDX-License-Identifier: MIT
#include "platform/restart.h"

#include <QCoreApplication>
#include <QProcess>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace pou {

RestartCommand restartCommand(qint64 pid, const QString &program, const QStringList &arguments,
                              std::chrono::seconds limit)
{
    // five tries a second
    const QString wait = u"tries=$1; shift; "
                         "while [ \"$tries\" -gt 0 ] && kill -0 \"$0\" 2>/dev/null; do "
                         "sleep 0.2; tries=$((tries - 1)); done; "
                         "exec \"$@\""_s;
    return {.program = u"/bin/sh"_s,
            .arguments = QStringList{u"-c"_s, wait, QString::number(pid), QString::number(limit.count() * 5),
                                     program} +
                         arguments};
}

bool startAgainAfterExit()
{
    // an AppImage is started by its own path, which the runtime hands on
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    const QString program = appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
    QStringList arguments = QCoreApplication::arguments();
    if (!arguments.isEmpty())
        arguments.removeFirst();
#ifdef Q_OS_WIN
    // the setup program starts it there
    Q_UNUSED(program)
    return false;
#else
    // a process that never goes, a zombie nobody reaps or a reused id, would
    // keep the new one waiting for good
    const RestartCommand command =
        restartCommand(QCoreApplication::applicationPid(), program, arguments, 60s);
    return QProcess::startDetached(command.program, command.arguments);
#endif
}

} // namespace pou
