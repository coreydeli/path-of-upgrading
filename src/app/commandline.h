// SPDX-License-Identifier: MIT
#pragma once

#include <QCommandLineParser>
#include <QString>
#include <QStringList>

namespace pou {

// What the program can be started with, which --help lists.
class CommandLine
{
public:
    CommandLine();

    // The program's own arguments, the way QCoreApplication has them, with
    // the program first. False, with errorText(), for any it doesn't know.
    bool parse(const QStringList &arguments);
    // Like parse(), but it prints the help or the version for --help and
    // --version, and what's wrong with the others, and then exits.
    void process(const QStringList &arguments);

    // Starts with only the tray icon, as it does when you log in.
    bool background() const;
    QString errorText() const;
    QString helpText();

    // What starts it that way, as the autostart entry runs it.
    static QString backgroundCommand();

private:
    QCommandLineParser m_parser;
};

} // namespace pou
