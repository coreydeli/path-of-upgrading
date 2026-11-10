// SPDX-License-Identifier: MIT
#include "app/commandline.h"

#include <QCoreApplication>
#include <QDir>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

const QString backgroundOption = u"background"_s;
const QString setupOption = u"setup"_s;

} // namespace

CommandLine::CommandLine()
{
    m_parser.setApplicationDescription(u"Checks Path of Exile 2 items against your build."_s);
    m_parser.addHelpOption();
    m_parser.addVersionOption();
    m_parser.addOption({backgroundOption, u"Start with only the tray icon, without the window."_s});
    m_parser.addOption({setupOption, u"Show the setup guide again."_s});
}

bool CommandLine::parse(const QStringList &arguments)
{
    return m_parser.parse(arguments);
}

void CommandLine::process(const QStringList &arguments)
{
    m_parser.process(arguments);
}

bool CommandLine::background() const
{
    return m_parser.isSet(backgroundOption);
}

bool CommandLine::setup() const
{
    return m_parser.isSet(setupOption);
}

QString CommandLine::errorText() const
{
    return m_parser.errorText();
}

QString CommandLine::helpText()
{
    return m_parser.helpText();
}

#ifndef Q_OS_WIN
namespace {

// A path for an Exec line, quoted as the desktop entry spec asks
QString quoted(const QString &path)
{
    QString text = u"\""_s;
    for (const QChar c : path) {
        if (c == u'"' || c == u'`' || c == u'$' || c == u'\\')
            text += u'\\';
        text += c;
    }
    return text + u'"';
}

} // namespace
#endif

QString CommandLine::backgroundCommand()
{
#ifdef Q_OS_WIN
    // which Windows runs by its whole path
    const QString program = u'"' + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + u'"';
#else
    // An AppImage runs from a mount that's gone when it exits, so the entry
    // has to start the file itself.
    const QString image = qEnvironmentVariable("APPIMAGE");
    const QString program = image.isEmpty() ? QCoreApplication::applicationName() : quoted(image);
#endif
    return program + u" --"_s + backgroundOption;
}

} // namespace pou
