// SPDX-License-Identifier: MIT
#include "platform/restart.h"

#include <QCoreApplication>
#include <QProcess>
#include <QStringList>

using namespace Qt::Literals::StringLiterals;

namespace pou {

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
    const QString wait = u"while kill -0 \"$0\" 2>/dev/null; do sleep 0.2; done; exec \"$@\""_s;
    return QProcess::startDetached(
        u"/bin/sh"_s,
        QStringList{u"-c"_s, wait, QString::number(QCoreApplication::applicationPid()), program} + arguments);
#endif
}

} // namespace pou
