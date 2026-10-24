// SPDX-License-Identifier: MIT
#include "app/settingsstore.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestSettingsStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void firstRun()
    {
        QTemporaryDir dir;
        const SettingsStore store(dir.filePath(u"settings.conf"_s));
        QCOMPARE(store.settings(), Settings());
    }

    void saved()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"settings.conf"_s);
        SettingsStore store(path);
        QSignalSpy changed(&store, &SettingsStore::changed);
        Settings settings;
        settings.character = u"torvald"_s;
        settings.overlay.hotkey = u"F3"_s;
        QVERIFY(store.set(settings));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(store.settings(), settings);
        QCOMPARE(SettingsStore(path).settings(), settings);
    }

    void same()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"settings.conf"_s);
        SettingsStore store(path);
        QSignalSpy changed(&store, &SettingsStore::changed);
        QVERIFY(store.set(Settings()));
        QVERIFY(changed.isEmpty());
        QVERIFY(!QFile::exists(path));
    }

    void cantSave()
    {
        // in a folder where a file is
        QTemporaryDir dir;
        QFile file(dir.filePath(u"config"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        SettingsStore store(dir.filePath(u"config/settings.conf"_s));
        QSignalSpy changed(&store, &SettingsStore::changed);
        Settings settings;
        settings.overlay.paused = true;
        QVERIFY(!store.set(settings));
        QCOMPARE(changed.size(), 1);
        QVERIFY(store.settings().overlay.paused);
    }
};

QTEST_GUILESS_MAIN(TestSettingsStore)
#include "tst_settingsstore.moc"
