// SPDX-License-Identifier: MIT
#include "core/settings.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <functional>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults()
    {
        QTemporaryDir dir;
        const QSettings empty(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        QCOMPARE(loadSettings(empty), Settings());
    }

    void roundTrip()
    {
        Settings s;
        s.character = u"0b6c1d52-4e8a-4f43-9a63-2f7e5e1c8d10"_s;
        s.trade.enabled = false;
        s.trade.league = {u"Runes of Aldur"_s, true, true};
        s.trade.status = ListingStatus::Online;
        s.trade.budget = {2, 15, Currency::Exalted, 410};
        s.trade.autoRate = false;
        s.pob.enabled = false;
        s.pob.location = u"/opt/path-of-building"_s;
        s.pob.damageWeight = 0.3;
        s.pob.priorities = {false, true, false, false, false};
        s.overlay.hotkey = u"F3"_s;
        s.overlay.resultsHotkey = u"Ctrl+Shift+R"_s;
        s.overlay.advancedCopy = true;
        s.overlay.watchClipboard = false;
        s.overlay.position = PopupPosition::Top;
        s.overlay.scale = 1.15;
        s.overlay.compact = false;
        s.overlay.colorblind = true;
        s.overlay.autoHideSeconds = 30;
        s.overlay.paused = true;

        QTemporaryDir dir;
        const QString path = dir.filePath(u"settings.conf"_s);
        {
            QSettings store(path, QSettings::IniFormat);
            saveSettings(store, s);
        }
        const QSettings again(path, QSettings::IniFormat);
        QCOMPARE(loadSettings(again), s);
        // readable by people too
        QCOMPARE(again.value(u"trade/league"_s).toString(), u"Runes of Aldur"_s);
        QCOMPARE(again.value(u"overlay/position"_s).toString(), u"top"_s);
    }

    void outOfRange()
    {
        QTemporaryDir dir;
        QSettings store(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        store.setValue(u"overlay/autoHideSeconds"_s, 1000);
        store.setValue(u"overlay/scale"_s, u"huge"_s);
        store.setValue(u"pob/damageWeight"_s, -2);
        store.setValue(u"trade/budgetMax"_s, u"nan"_s);
        store.setValue(u"trade/status"_s, u"whenever"_s);
        store.setValue(u"overlay/position"_s, u"sideways"_s);

        const Settings s = loadSettings(store);
        QCOMPARE(s.overlay.autoHideSeconds, 120);
        QCOMPARE(s.overlay.scale, 1.0);
        QCOMPARE(s.pob.damageWeight, 0.0);
        QCOMPARE(s.trade.budget.max, 0.0);
        QCOMPARE(s.trade.status, ListingStatus::Available);
        QCOMPARE(s.overlay.position, PopupPosition::Cursor);
    }

    void tradeOff_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<bool>("soloSelfFound");
        QTest::addColumn<QString>("why");
        const QString off = u"Trade searches are turned off in the settings."_s;
        const QString solo = u"There's no trading in solo self-found, so trade searches are off."_s;
        QTest::newRow("on") << true << false << QString();
        QTest::newRow("turned off") << false << false << off;
        QTest::newRow("solo self-found") << true << true << solo;
        QTest::newRow("both") << false << true << off;
    }

    void tradeOff()
    {
        QFETCH(bool, enabled);
        QFETCH(bool, soloSelfFound);
        QFETCH(QString, why);
        TradeSettings trade;
        trade.enabled = enabled;
        trade.league.soloSelfFound = soloSelfFound;
        QCOMPARE(tradeUnavailable(trade), why);
    }

    void everySettingHasItsOwnKey()
    {
        const QList<std::function<void(Settings &)>> changes = {
            [](Settings &s) { s.character = u"abc"_s; },
            [](Settings &s) { s.trade.enabled = false; },
            [](Settings &s) { s.trade.league.name = u"Standard"_s; },
            [](Settings &s) { s.trade.league.hardcore = true; },
            [](Settings &s) { s.trade.league.soloSelfFound = true; },
            [](Settings &s) { s.trade.status = ListingStatus::Online; },
            [](Settings &s) { s.trade.budget.min = 3; },
            [](Settings &s) { s.trade.budget.max = 9; },
            [](Settings &s) { s.trade.budget.currency = Currency::Chaos; },
            [](Settings &s) { s.trade.budget.divineRate = 250; },
            [](Settings &s) { s.trade.autoRate = false; },
            [](Settings &s) { s.pob.enabled = false; },
            [](Settings &s) { s.pob.location = u"/opt/pob"_s; },
            [](Settings &s) { s.pob.damageWeight = 0.25; },
            [](Settings &s) { s.pob.priorities.resistsFirst = false; },
            [](Settings &s) { s.pob.priorities.chaos = true; },
            [](Settings &s) { s.pob.priorities.keepDamage = false; },
            [](Settings &s) { s.pob.priorities.spirit = false; },
            [](Settings &s) { s.pob.priorities.attributes = false; },
            [](Settings &s) { s.overlay.hotkey = u"F5"_s; },
            [](Settings &s) { s.overlay.resultsHotkey = u"F6"_s; },
            [](Settings &s) { s.overlay.advancedCopy = true; },
            [](Settings &s) { s.overlay.watchClipboard = false; },
            [](Settings &s) { s.overlay.position = PopupPosition::Top; },
            [](Settings &s) { s.overlay.scale = 1.5; },
            [](Settings &s) { s.overlay.compact = false; },
            [](Settings &s) { s.overlay.colorblind = true; },
            [](Settings &s) { s.overlay.autoHideSeconds = 30; },
            [](Settings &s) { s.overlay.paused = true; },
        };

        // changing one setting must change only that one when it comes back
        int row = 0;
        for (const auto &change : changes) {
            const QString which = u"change "_s + QString::number(row++);
            Settings changed;
            change(changed);
            QVERIFY2(changed != Settings(), qPrintable(which));

            QTemporaryDir dir;
            const QString path = dir.filePath(u"settings.conf"_s);
            {
                QSettings store(path, QSettings::IniFormat);
                saveSettings(store, changed);
            }
            const QSettings again(path, QSettings::IniFormat);
            QVERIFY2(loadSettings(again) == changed, qPrintable(which));
        }
    }

    void numbersStayWithinTheirLimits()
    {
        QTemporaryDir dir;
        QSettings store(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        store.setValue(u"overlay/autoHideSeconds"_s, 0);
        store.setValue(u"overlay/scale"_s, 0.1);
        store.setValue(u"pob/damageWeight"_s, 3);
        store.setValue(u"trade/budgetMin"_s, -5);
        store.setValue(u"trade/budgetMax"_s, -5);
        store.setValue(u"trade/divineRate"_s, -5);
        Settings s = loadSettings(store);
        QCOMPARE(s.overlay.autoHideSeconds, 2);
        QCOMPARE(s.overlay.scale, 0.5);
        QCOMPARE(s.pob.damageWeight, 1.0);
        QCOMPARE(s.trade.budget.min, 0.0);
        QCOMPARE(s.trade.budget.max, 0.0);
        QCOMPARE(s.trade.budget.divineRate, 0.0);

        store.setValue(u"overlay/scale"_s, 10);
        store.setValue(u"overlay/autoHideSeconds"_s, u"soon"_s);
        s = loadSettings(store);
        QCOMPARE(s.overlay.scale, 2.0);
        QCOMPARE(s.overlay.autoHideSeconds, OverlaySettings().autoHideSeconds);
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
