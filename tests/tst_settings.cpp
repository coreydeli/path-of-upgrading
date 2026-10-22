// SPDX-License-Identifier: MIT
#include "core/settings.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

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
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
