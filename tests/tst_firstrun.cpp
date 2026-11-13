// SPDX-License-Identifier: MIT
#include "core/firstrun.h"
#include "services/settingsfile.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <functional>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// What the program leaves in the settings by itself on the first start, as
// the beta or the release starts it.
Settings firstStart(bool beta)
{
    Settings s;
    s.trade.enabled = beta;
    s.updates.includeBetas = beta;
    return s;
}

} // namespace

class TestFirstRun : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nothingSavedYet_data()
    {
        QTest::addColumn<bool>("beta");
        QTest::newRow("beta") << true;
        QTest::newRow("release") << false;
    }

    void nothingSavedYet()
    {
        QFETCH(bool, beta);
        QVERIFY(setupWanted(firstStart(beta), false, beta));
    }

    void whatTheProgramWritesItself()
    {
        // after a start that was quit before the guide was done
        Settings s = firstStart(false);
        s.overlay.startChosen = true;
        s.overlay.hotkey = u"F3"_s;
        s.overlay.resultsHotkey = u"Ctrl+Shift+F3"_s;
        QVERIFY(setupWanted(s, false, false));
        s.overlay.hotkey = QString();
        QVERIFY(setupWanted(s, false, false));
    }

    void finishedOrSkipped()
    {
        Settings s = firstStart(false);
        s.setupDone = true;
        QVERIFY(!setupWanted(s, false, false));
    }

    void charactersAreSaved() { QVERIFY(!setupWanted(firstStart(false), true, false)); }

    void tradeSearchesChosen()
    {
        // on in a release, or off in a beta, is a choice
        Settings release = firstStart(false);
        release.trade.enabled = true;
        QVERIFY(!setupWanted(release, false, false));
        Settings beta = firstStart(true);
        beta.trade.enabled = false;
        QVERIFY(!setupWanted(beta, false, true));
    }

    void anythingElseChanged_data()
    {
        QTest::addColumn<std::function<void(Settings &)>>("change");
        using Change = std::function<void(Settings &)>;
        QTest::newRow("character") << Change([](Settings &s) { s.character = u"abc"_s; });
        QTest::newRow("league") << Change([](Settings &s) { s.trade.league.name = u"Standard"_s; });
        QTest::newRow("hardcore") << Change([](Settings &s) { s.trade.league.hardcore = true; });
        QTest::newRow("solo self-found") << Change([](Settings &s) { s.trade.league.soloSelfFound = true; });
        QTest::newRow("budget") << Change([](Settings &s) { s.trade.budget.max = 9; });
        QTest::newRow("path of building") << Change([](Settings &s) { s.pob.enabled = false; });
        QTest::newRow("popup") << Change([](Settings &s) { s.overlay.scale = 1.5; });
        QTest::newRow("paused") << Change([](Settings &s) { s.overlay.paused = true; });
        QTest::newRow("appearance") << Change([](Settings &s) { s.appearance.textSize = 12; });
    }

    void anythingElseChanged()
    {
        QFETCH(std::function<void(Settings &)>, change);
        Settings s = firstStart(false);
        change(s);
        QVERIFY(!setupWanted(s, false, false));
    }

    void readFromTheFile()
    {
        // a file written by an earlier version has no setup key at all
        QTemporaryDir dir;
        QSettings store(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        store.setValue(u"overlay/hotkey"_s, u"F2"_s);
        QVERIFY(setupWanted(loadSettings(store, false), false, false));
        store.setValue(u"app/setupDone"_s, true);
        QVERIFY(!setupWanted(loadSettings(store, false), false, false));
        store.setValue(u"app/setupDone"_s, false);
        store.setValue(u"trade/league"_s, u"Standard"_s);
        QVERIFY(!setupWanted(loadSettings(store, false), false, false));
    }
};

QTEST_GUILESS_MAIN(TestFirstRun)
#include "tst_firstrun.moc"
