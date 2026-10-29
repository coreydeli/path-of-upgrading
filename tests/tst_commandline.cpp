// SPDX-License-Identifier: MIT
#include "app/commandline.h"

#include <QProcess>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestCommandLine : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nothing()
    {
        CommandLine line;
        QVERIFY(line.parse({u"path-of-upgrading"_s}));
        QVERIFY(!line.background());
    }

    void background()
    {
        CommandLine line;
        QVERIFY(line.parse({u"path-of-upgrading"_s, u"--background"_s}));
        QVERIFY(line.background());
    }

    void unknown()
    {
        CommandLine line;
        QVERIFY(!line.parse({u"path-of-upgrading"_s, u"--bogus"_s}));
        QVERIFY(line.errorText().contains(u"bogus"_s));
    }

    void whatTheEntryRuns()
    {
        // which the program has to understand
        CommandLine line;
        QVERIFY(line.parse(QProcess::splitCommand(CommandLine::backgroundCommand())));
        QVERIFY(line.background());
        QVERIFY(CommandLine::backgroundCommand().endsWith(u" --background"_s));
    }

    void help()
    {
        CommandLine line;
        const QString help = line.helpText();
        QVERIFY(help.contains(u"--background"_s));
        QVERIFY(help.contains(u"--help"_s));
        QVERIFY(help.contains(u"--version"_s));
        QVERIFY(help.contains(u"Checks Path of Exile 2 items"_s));
    }
};

QTEST_GUILESS_MAIN(TestCommandLine)
#include "tst_commandline.moc"
