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
        Settings expected;
        expected.trade.enabled = isBetaBuild();
        QCOMPARE(loadSettings(empty), expected);
    }

    void theBuildSaysIfItIsABeta()
    {
        // from the CMake option the build was made with
        QCOMPARE(isBetaBuild(), POU_EXPECT_BETA == 1);
    }

    void tradeStartsByTheBuild()
    {
        QTemporaryDir dir;
        const QSettings empty(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        QVERIFY(loadSettings(empty, true).trade.enabled);
        QVERIFY(!loadSettings(empty, false).trade.enabled);
    }

    void tradeChoiceIsKept()
    {
        QTemporaryDir dir;
        QSettings store(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        store.setValue(u"trade/enabled"_s, true);
        QVERIFY(loadSettings(store, false).trade.enabled);
        store.setValue(u"trade/enabled"_s, false);
        QVERIFY(!loadSettings(store, true).trade.enabled);
    }

    void tradeNotChosenIsNotSaved()
    {
        // so a beta and a release each start it their own way
        for (const bool beta : {false, true}) {
            QTemporaryDir dir;
            QSettings store(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
            Settings s = loadSettings(store, beta);
            s.overlay.hotkey = u"F5"_s;
            saveSettings(store, s, beta);
            QVERIFY(!store.contains(u"trade/enabled"_s));
            QCOMPARE(store.value(u"overlay/hotkey"_s).toString(), u"F5"_s);
            QCOMPARE(loadSettings(store, !beta).trade.enabled, !beta);
        }
    }

    void tradeChoiceIsSaved()
    {
        for (const bool beta : {false, true}) {
            QTemporaryDir dir;
            QSettings store(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
            Settings s = loadSettings(store, beta);
            s.trade.enabled = !beta;
            saveSettings(store, s, beta);
            QCOMPARE(store.value(u"trade/enabled"_s).toBool(), !beta);
            QCOMPARE(loadSettings(store, !beta).trade.enabled, !beta);

            // and changing your mind back to what the build starts with is
            // a choice too
            s.trade.enabled = beta;
            saveSettings(store, s, beta);
            QCOMPARE(store.value(u"trade/enabled"_s).toBool(), beta);
            QCOMPARE(loadSettings(store, !beta).trade.enabled, beta);
        }
    }

    void popupStaysAMinuteUnlessSet()
    {
        QTemporaryDir dir;
        QSettings store(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        QCOMPARE(loadSettings(store).overlay.autoHideSeconds, 60);
        // a saved one is kept, even the old default
        store.setValue(u"overlay/autoHideSeconds"_s, 12);
        QCOMPARE(loadSettings(store).overlay.autoHideSeconds, 12);
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
        s.trade.rateMinutes = 25;
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
        s.appearance.colors = ColorTheme::Desktop;
        s.appearance.decorations = Decorations::Plain;
        s.appearance.headingFont = u"Palatino"_s;
        s.appearance.textFont = u"Sans Mono"_s;
        s.appearance.textSize = 11.5;
        s.overlay.startChosen = true;

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
        QCOMPARE(again.value(u"appearance/theme"_s).toString(), u"desktop"_s);
        QCOMPARE(again.value(u"appearance/decorations"_s).toString(), u"plain"_s);
        QCOMPARE(again.value(u"appearance/headingFont"_s).toString(), u"Palatino"_s);
        QCOMPARE(again.value(u"appearance/textFont"_s).toString(), u"Sans Mono"_s);
        QCOMPARE(again.value(u"appearance/textSize"_s).toDouble(), 11.5);
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
        store.setValue(u"appearance/theme"_s, u"neon"_s);
        store.setValue(u"appearance/decorations"_s, u"lots"_s);
        store.setValue(u"appearance/textSize"_s, 500);

        const Settings s = loadSettings(store);
        QCOMPARE(s.overlay.autoHideSeconds, 120);
        QCOMPARE(s.overlay.scale, 1.0);
        QCOMPARE(s.pob.damageWeight, 0.0);
        QCOMPARE(s.trade.budget.max, 0.0);
        QCOMPARE(s.trade.status, ListingStatus::Available);
        QCOMPARE(s.overlay.position, PopupPosition::Cursor);
        QCOMPARE(s.appearance.colors, ColorTheme::App);
        QCOMPARE(s.appearance.decorations, Decorations::Full);
        QCOMPARE(s.appearance.textSize, largestTextSize);
    }

    void textSizeIsReadable_data()
    {
        QTest::addColumn<QVariant>("saved");
        QTest::addColumn<double>("size");
        QTest::newRow("desktop's") << QVariant(0) << 0.0;
        QTest::newRow("negative") << QVariant(-4) << 0.0;
        QTest::newRow("too small") << QVariant(2.5) << smallestTextSize;
        QTest::newRow("between") << QVariant(13.5) << 13.5;
        QTest::newRow("words") << QVariant(u"big"_s) << 0.0;
        QTest::newRow("missing") << QVariant() << 0.0;
    }

    void textSizeIsReadable()
    {
        QFETCH(QVariant, saved);
        QFETCH(double, size);
        QTemporaryDir dir;
        QSettings store(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        if (saved.isValid())
            store.setValue(u"appearance/textSize"_s, saved);
        QCOMPARE(loadSettings(store).appearance.textSize, size);
    }

    void tradeOff_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<bool>("soloSelfFound");
        QTest::addColumn<bool>("beta");
        QTest::addColumn<QString>("why");
        const QString how = u"Turn them on with Search the trade site, on the Trade page."_s;
        const QString off = u"Trade searches are turned off. "_s + how;
        const QString testing =
            u"Trade searches are off in this release while they're still being tested. "_s + how;
        const QString solo = u"There's no trading in solo self-found, so trade searches are off."_s;
        QTest::newRow("on") << true << false << false << QString();
        QTest::newRow("on in a beta") << true << false << true << QString();
        QTest::newRow("turned off in a beta") << false << false << true << off;
        QTest::newRow("off in a release") << false << false << false << testing;
        QTest::newRow("solo self-found") << true << true << false << solo;
        QTest::newRow("both") << false << true << true << off;
    }

    void tradeOff()
    {
        QFETCH(bool, enabled);
        QFETCH(bool, soloSelfFound);
        QFETCH(bool, beta);
        QFETCH(QString, why);
        TradeSettings trade;
        trade.enabled = enabled;
        trade.league.soloSelfFound = soloSelfFound;
        QCOMPARE(tradeUnavailable(trade, beta), why);
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
            [](Settings &s) { s.trade.rateMinutes = 30; },
            [](Settings &s) { s.pob.enabled = false; },
            [](Settings &s) { s.pob.location = u"/opt/pob"_s; },
            [](Settings &s) { s.pob.damageWeight = 0.25; },
            [](Settings &s) { s.pob.priorities.resistsFirst = false; },
            [](Settings &s) { s.pob.priorities.chaos = true; },
            [](Settings &s) { s.pob.priorities.keepDamage = false; },
            [](Settings &s) { s.pob.priorities.keepDefence = false; },
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
            [](Settings &s) { s.appearance.colors = ColorTheme::Desktop; },
            [](Settings &s) { s.appearance.decorations = Decorations::Plain; },
            [](Settings &s) { s.appearance.headingFont = u"text"_s; },
            [](Settings &s) { s.appearance.textFont = u"Sans Mono"_s; },
            [](Settings &s) { s.appearance.textSize = 12; },
            [](Settings &s) { s.overlay.startChosen = true; },
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
        store.setValue(u"trade/rateMinutes"_s, 0);
        Settings s = loadSettings(store);
        QCOMPARE(s.overlay.autoHideSeconds, 2);
        QCOMPARE(s.overlay.scale, 0.5);
        QCOMPARE(s.pob.damageWeight, 1.0);
        QCOMPARE(s.trade.budget.min, 0.0);
        QCOMPARE(s.trade.budget.max, 0.0);
        QCOMPARE(s.trade.budget.divineRate, 0.0);
        QCOMPARE(s.trade.rateMinutes, 1);

        store.setValue(u"overlay/scale"_s, 10);
        store.setValue(u"overlay/autoHideSeconds"_s, u"soon"_s);
        store.setValue(u"trade/rateMinutes"_s, 600);
        s = loadSettings(store);
        QCOMPARE(s.trade.rateMinutes, 60);
        store.setValue(u"trade/rateMinutes"_s, u"often"_s);
        s = loadSettings(store);
        QCOMPARE(s.trade.rateMinutes, TradeSettings().rateMinutes);
        QCOMPARE(s.overlay.scale, 2.0);
        QCOMPARE(s.overlay.autoHideSeconds, OverlaySettings().autoHideSeconds);
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
