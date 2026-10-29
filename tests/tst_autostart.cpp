// SPDX-License-Identifier: MIT
#include "platform/autostart.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const Autostart::Entry entry{
    .name = u"Made-up Program"_s, .exec = u"made-up --background"_s, .icon = u"made-up-icon"_s};

QByteArray contents(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool write(const QString &path, const QByteArray &text)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(text) == text.size();
}

} // namespace

class TestAutostart : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void off()
    {
        QTemporaryDir dir;
        const Autostart autostart(dir.filePath(u"autostart/made-up.desktop"_s), entry);
        QVERIFY(!autostart.isEnabled());
    }

    void turnsOn()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"autostart/made-up.desktop"_s);
        Autostart autostart(file, entry);
        // which makes the folder
        QVERIFY(autostart.setEnabled(true));
        QVERIFY(autostart.errorString().isEmpty());
        QVERIFY(autostart.isEnabled());
        QCOMPARE(contents(file), QByteArray("[Desktop Entry]\n"
                                            "Type=Application\n"
                                            "Name=Made-up Program\n"
                                            "Exec=made-up --background\n"
                                            "Icon=made-up-icon\n"
                                            "Terminal=false\n"));
    }

    void turnsOff()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"autostart/made-up.desktop"_s);
        Autostart autostart(file, entry);
        QVERIFY(autostart.setEnabled(true));
        QVERIFY(autostart.setEnabled(false));
        QVERIFY(!autostart.isEnabled());
        QVERIFY(!QFile::exists(file));
    }

    void turningOffWhatIsOff()
    {
        QTemporaryDir dir;
        Autostart autostart(dir.filePath(u"autostart/made-up.desktop"_s), entry);
        QVERIFY(autostart.setEnabled(false));
        QVERIFY(autostart.errorString().isEmpty());
    }

    void sameAgain()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"autostart/made-up.desktop"_s);
        Autostart autostart(file, entry);
        QVERIFY(autostart.setEnabled(true));
        const QByteArray first = contents(file);
        QVERIFY(autostart.setEnabled(true));
        QCOMPARE(contents(file), first);
    }

    void removedByHand()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"autostart/made-up.desktop"_s);
        Autostart autostart(file, entry);
        QVERIFY(autostart.setEnabled(true));
        QVERIFY(QFile::remove(file));
        QVERIFY(!autostart.isEnabled());
        // and it can be turned on again
        QVERIFY(autostart.setEnabled(true));
        QVERIFY(autostart.isEnabled());
    }

    void foundWhenItStarts()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"autostart/made-up.desktop"_s);
        QVERIFY(Autostart(file, entry).setEnabled(true));
        QVERIFY(Autostart(file, entry).isEnabled());
    }

    void hiddenIsOff()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"made-up.desktop"_s);
        QVERIFY(write(file, "[Desktop Entry]\nType=Application\nExec=made-up\nHidden=true\n"));
        Autostart autostart(file, entry);
        QVERIFY(!autostart.isEnabled());

        // turning it on writes it again, without that
        QVERIFY(autostart.setEnabled(true));
        QVERIFY(autostart.isEnabled());
        QVERIFY(!contents(file).contains("Hidden"));
    }

    void notHidden()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"made-up.desktop"_s);
        QVERIFY(write(file, "[Desktop Entry]\nType=Application\nExec=made-up\nHidden=false\n"));
        QVERIFY(Autostart(file, entry).isEnabled());
    }

    void noFile()
    {
        Autostart autostart(QString(), entry);
        QVERIFY(!autostart.isEnabled());
        QVERIFY(!autostart.setEnabled(true));
        QCOMPARE(autostart.errorString(), u"There's nowhere to keep it."_s);
        QVERIFY(!autostart.isEnabled());
        // and off is as it is
        QVERIFY(autostart.setEnabled(false));
        QVERIFY(autostart.errorString().isEmpty());
    }

    void cantWrite()
    {
        QTemporaryDir dir;
        // a file where the folder should be
        const QString blocker = dir.filePath(u"autostart"_s);
        QVERIFY(write(blocker, "not a folder"));
        Autostart autostart(blocker + u"/made-up.desktop"_s, entry);
        QVERIFY(!autostart.setEnabled(true));
        QVERIFY(autostart.errorString().contains(u"made-up.desktop"_s));
        QVERIFY(!autostart.isEnabled());

        // and the error is gone with the next that works
        QVERIFY(autostart.setEnabled(false));
        QVERIFY(autostart.errorString().isEmpty());
    }

    void cantRemove()
    {
        QTemporaryDir dir;
        // a folder in its place can't be removed as a file
        const QString folder = dir.filePath(u"made-up.desktop"_s);
        QVERIFY(QDir().mkpath(folder + u"/inside"_s));
        Autostart autostart(folder, entry);
        QVERIFY(!autostart.setEnabled(false));
        QVERIFY(autostart.errorString().contains(u"made-up.desktop"_s));
    }

    void inTheConfigFolder()
    {
        QTemporaryDir dir;
        qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
        QCOMPARE(Autostart::fileFor(u"io.example.Made-up"_s),
                 dir.filePath(u"autostart/io.example.Made-up.desktop"_s));
        qunsetenv("XDG_CONFIG_HOME");
    }
};

QTEST_GUILESS_MAIN(TestAutostart)
#include "tst_autostart.moc"
