// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>
#include <chrono>

namespace pou {

// Starts this program again, once this one has gone: it waits for the process
// to end, so the new one isn't turned away as a second copy. The caller quits.
// False when it couldn't be started.
bool startAgainAfterExit();

// What starts a program once the process with this id has ended, or once the
// limit is up, whichever comes first. Past the limit the new one starts
// anyway, and hands over to the old one should it still be there.
struct RestartCommand
{
    QString program;
    QStringList arguments;
};
RestartCommand restartCommand(qint64 pid, const QString &program, const QStringList &arguments,
                              std::chrono::seconds limit);

} // namespace pou
