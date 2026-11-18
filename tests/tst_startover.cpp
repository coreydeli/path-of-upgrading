// SPDX-License-Identifier: MIT
#include "app/startover.h"
#include "app/tradesession.h"
#include "fakeglobalshortcuts.h"
#include "fakesecretstore.h"
#include "platform/autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

bool write(const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("made up") > 0;
}

// Made-up folders with something in each, a made-up session ID in a fake
// keyring and a fake desktop.
struct Setup
{
    QTemporaryDir dir;
    FakeSecretStore keyring;
    FakeGlobalShortcuts shortcuts;
    Autostart autostart{dir.filePath(u"autostart/made-up.desktop"_s),
                        {.name = u"Made-up"_s, .exec = u"made-up"_s}};
    // everything is in the temporary folder, and named for the marker, which
    // is all the removal will touch
    StartOver::Places places{.settings = dir.filePath(u"made-up-apprc"_s),
                             .data = dir.filePath(u"made-up-app"_s),
                             .cache = dir.filePath(u"made-up-app-cache"_s),
                             .marker = u"made-up-app"_s};
    int restarts = 0;

    Setup()
    {
        keyring.secrets.insert(TradeSession::secretName(), u"not-a-real-id"_s);
        shortcuts.add(u"check"_s, u"Check"_s, QKeySequence(Qt::Key_F2));
        if (!write(places.settings) || !write(places.data + u"/characters/one.json"_s) ||
            !write(places.data + u"/history.json"_s) || !write(places.cache + u"/trade/stats.json"_s) ||
            !write(dir.filePath(u"pob/Builds/mine.xml"_s)) || !write(dir.filePath(u"made-up-neighbour"_s)) ||
            !autostart.setEnabled(true))
            qFatal("Couldn't make the made-up files.");
    }

    std::unique_ptr<StartOver> make(bool offered = true)
    {
        auto start = std::make_unique<StartOver>(offered, places, &autostart, &keyring, &shortcuts);
        start->setRestart([this] {
            ++restarts;
            return true;
        });
        return start;
    }

    bool finished(const StartOver &start) const
    {
        return QTest::qWaitFor([&] { return !start.isBusy(); }, 5000);
    }
};

} // namespace

class TestStartOver : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void wipesEverythingItKeepsAndStartsAgain()
    {
        Setup s;
        const auto start = s.make();
        QVERIFY(s.autostart.isEnabled());

        start->run();
        QVERIFY(start->isBusy());
        QVERIFY(s.finished(*start));

        QVERIFY(start->problems().isEmpty());
        QVERIFY(!QFileInfo::exists(s.places.settings));
        QVERIFY(!QFileInfo::exists(s.places.data));
        QVERIFY(!QFileInfo::exists(s.places.cache));
        QVERIFY(!s.autostart.isEnabled());
        QVERIFY(!QFileInfo::exists(s.dir.filePath(u"autostart/made-up.desktop"_s)));
        QVERIFY(!s.keyring.secrets.contains(TradeSession::secretName()));
        QCOMPARE(s.shortcuts.forgets, 1);
        QCOMPARE(s.restarts, 1);
        // nothing that isn't the app's
        QVERIFY(QFileInfo::exists(s.dir.filePath(u"pob/Builds/mine.xml"_s)));
        QVERIFY(QFileInfo::exists(s.dir.filePath(u"made-up-neighbour"_s)));
    }

    void aReleaseDoesNothing()
    {
        Setup s;
        const auto start = s.make(false);
        QVERIFY(!start->isOffered());
        start->run();
        QVERIFY(!start->isBusy());
        QTest::qWait(50);
        QVERIFY(QFileInfo::exists(s.places.settings));
        QVERIFY(QFileInfo::exists(s.places.data));
        QVERIFY(s.keyring.secrets.contains(TradeSession::secretName()));
        QVERIFY(s.autostart.isEnabled());
        QCOMPARE(s.keyring.removals, 0);
        QCOMPARE(s.shortcuts.forgets, 0);
        QCOMPARE(s.restarts, 0);
    }

    void aLockedKeyringIsSaidAndTheRestStillGoes()
    {
        Setup s;
        s.keyring.failure = u"The keyring is locked."_s;
        const auto start = s.make();

        start->run();
        QVERIFY(s.finished(*start));

        QCOMPARE(start->problems().size(), 1);
        QVERIFY(start->problems().first().contains(u"keyring"_s));
        QVERIFY(start->problems().first().contains(u"The keyring is locked."_s));
        QVERIFY(!QFileInfo::exists(s.places.settings));
        QVERIFY(!QFileInfo::exists(s.places.data));
        QVERIFY(!QFileInfo::exists(s.places.cache));
        QVERIFY(!s.autostart.isEnabled());
        QCOMPARE(s.shortcuts.forgets, 1);
        // it stays, to say what's left
        QCOMPARE(s.restarts, 0);
    }

    void aFileThatWontGoIsSaidAndTheRestStillGoes()
    {
        Setup s;
        // a folder where the settings file should be, with the folder's own
        // folder read only, so it can't be taken out
        const QString locked = s.dir.filePath(u"locked"_s);
        QVERIFY(write(locked + u"/made-up-apprc/inside"_s));
        s.places.settings = locked + u"/made-up-apprc"_s;
        QVERIFY(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        const auto restore = qScopeGuard([&] {
            QFile::setPermissions(locked,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        });
        if (QFileInfo(locked).isWritable() || QFile::remove(locked + u"/probe"_s))
            QSKIP("This account can write to a folder that's read only.");
        const auto start = s.make();

        start->run();
        QVERIFY(s.finished(*start));

        QCOMPARE(start->problems().size(), 1);
        QVERIFY(start->problems().first().contains(u"your settings"_s));
        QVERIFY(!QFileInfo::exists(s.places.data));
        QVERIFY(!QFileInfo::exists(s.places.cache));
        QVERIFY(!s.keyring.secrets.contains(TradeSession::secretName()));
        QCOMPARE(s.restarts, 0);
    }

    void aRestartThatFailsIsSaid()
    {
        Setup s;
        const auto start = s.make();
        start->setRestart([] { return false; });

        start->run();
        QVERIFY(s.finished(*start));

        QCOMPARE(start->problems().size(), 1);
        QVERIFY(start->problems().first().contains(u"start again"_s));
        QVERIFY(!QFileInfo::exists(s.places.settings));
    }

    void asksTheKeyringForTheSessionIdOnly()
    {
        Setup s;
        s.keyring.secrets.insert(u"something-else"_s, u"kept"_s);
        const auto start = s.make();
        start->run();
        QVERIFY(s.finished(*start));
        QVERIFY(s.keyring.secrets.contains(u"something-else"_s));
    }
};

QTEST_GUILESS_MAIN(TestStartOver)
#include "tst_startover.moc"
