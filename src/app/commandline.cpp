// SPDX-License-Identifier: MIT
#include "app/commandline.h"

#include <QCoreApplication>
#include <QDir>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

const QString backgroundOption = u"background"_s;

} // namespace

CommandLine::CommandLine()
{
    m_parser.setApplicationDescription(u"Checks Path of Exile 2 items against your build."_s);
    m_parser.addHelpOption();
    m_parser.addVersionOption();
    m_parser.addOption({backgroundOption, u"Start with only the tray icon, without the window."_s});
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

QString CommandLine::errorText() const
{
    return m_parser.errorText();
}

QString CommandLine::helpText()
{
    return m_parser.helpText();
}

QString CommandLine::backgroundCommand()
{
#ifdef Q_OS_WIN
    // which Windows runs by its whole path
    const QString program = u'"' + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + u'"';
#else
    const QString program = QCoreApplication::applicationName();
#endif
    return program + u" --"_s + backgroundOption;
}

} // namespace pou
