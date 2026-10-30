// SPDX-License-Identifier: MIT
#include "app/hotkeys.h"
#include "fakeglobalshortcuts.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

using Hotkey = Hotkeys::Hotkey;

class TestHotkeys : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void firstRun()
    {
        FakeGlobalShortcuts shortcuts;
        Hotkeys hotkeys(&shortcuts);
        hotkeys.add({});
        QCOMPARE(hotkeys.key(Hotkey::Check), u"F2"_s);
        QCOMPARE(hotkeys.key(Hotkey::Results), u"Ctrl+Shift+D"_s);
        QCOMPARE(shortcuts.names.value(u"check-item"_s), u"Check the item under the mouse"_s);
        QCOMPARE(shortcuts.names.value(u"trade-results"_s), u"Show the trade results for it"_s);
    }

    void yourKeys()
    {
        FakeGlobalShortcuts shortcuts;
        Hotkeys hotkeys(&shortcuts);
        OverlaySettings settings;
        settings.hotkey = u"F3"_s;
        settings.resultsHotkey = u"Alt+D"_s;
        hotkeys.add(settings);
        QCOMPARE(hotkeys.key(Hotkey::Check), u"F3"_s);
        QCOMPARE(hotkeys.key(Hotkey::Results), u"Alt+D"_s);
    }

    void desktopsKeys()
    {
        // the ones it had there, which it was given in System Settings
        FakeGlobalShortcuts shortcuts;
        shortcuts.keys.insert(u"check-item"_s, QKeySequence(u"F4"_s));
        Hotkeys hotkeys(&shortcuts);
        hotkeys.add({});
        QCOMPARE(hotkeys.key(Hotkey::Check), u"F4"_s);
        QCOMPARE(hotkeys.key(Hotkey::Results), u"Ctrl+Shift+D"_s);
    }

    void taken()
    {
        // by another program before it was added
        FakeGlobalShortcuts shortcuts;
        shortcuts.taken << QKeySequence(u"F2"_s);
        Hotkeys hotkeys(&shortcuts);
        hotkeys.add({});
        QCOMPARE(hotkeys.key(Hotkey::Check), QString());
    }

    void withoutTheService()
    {
        FakeGlobalShortcuts shortcuts;
        Hotkeys hotkeys(&shortcuts);
        QVERIFY(hotkeys.isAvailable());
        QCOMPARE(hotkeys.errorString(), QString());

        shortcuts.available = false;
        QVERIFY(!hotkeys.isAvailable());
        QCOMPARE(hotkeys.errorString(), u"It's off."_s);
    }

    void pressed()
    {
        FakeGlobalShortcuts shortcuts;
        Hotkeys hotkeys(&shortcuts);
        hotkeys.add({});
        QSignalSpy pressed(&hotkeys, &Hotkeys::pressed);
        Q_EMIT shortcuts.activated(u"trade-results"_s);
        Q_EMIT shortcuts.activated(u"check-item"_s);
        Q_EMIT shortcuts.activated(u"something-else"_s);
        QCOMPARE(pressed.size(), 2);
        QCOMPARE(pressed.at(0).at(0).value<Hotkey>(), Hotkey::Results);
        QCOMPARE(pressed.at(1).at(0).value<Hotkey>(), Hotkey::Check);
    }

    void setKey()
    {
        FakeGlobalShortcuts shortcuts;
        Hotkeys hotkeys(&shortcuts);
        hotkeys.add({});
        QSignalSpy changed(&hotkeys, &Hotkeys::keyChanged);
        QVERIFY(hotkeys.setKey(Hotkey::Check, u"Ctrl+F2"_s));
        QCOMPARE(hotkeys.key(Hotkey::Check), u"Ctrl+F2"_s);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().at(0).value<Hotkey>(), Hotkey::Check);

        // the one it has
        QVERIFY(hotkeys.setKey(Hotkey::Check, u"Ctrl+F2"_s));
        QCOMPARE(changed.size(), 1);

        QVERIFY(hotkeys.setKey(Hotkey::Results, {}));
        QCOMPARE(hotkeys.key(Hotkey::Results), QString());
        QCOMPARE(changed.size(), 2);
    }

    void notSet_data()
    {
        QTest::addColumn<QString>("key");
        QTest::newRow("another program's") << u"F5"_s;
        QTest::newRow("the other hotkey's") << u"Ctrl+Shift+D"_s;
        QTest::newRow("not a key") << u"Ctrl+Foo"_s;
    }

    void notSet()
    {
        QFETCH(QString, key);
        FakeGlobalShortcuts shortcuts;
        shortcuts.taken << QKeySequence(u"F5"_s);
        Hotkeys hotkeys(&shortcuts);
        hotkeys.add({});
        QSignalSpy changed(&hotkeys, &Hotkeys::keyChanged);
        QVERIFY(!hotkeys.setKey(Hotkey::Check, key));
        QCOMPARE(hotkeys.key(Hotkey::Check), u"F2"_s);
        QCOMPARE(changed.size(), 0);
    }

    void changedOnTheDesktop()
    {
        FakeGlobalShortcuts shortcuts;
        Hotkeys hotkeys(&shortcuts);
        hotkeys.add({});
        QSignalSpy changed(&hotkeys, &Hotkeys::keyChanged);
        shortcuts.change(u"trade-results"_s, QKeySequence(u"Alt+T"_s));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().at(0).value<Hotkey>(), Hotkey::Results);
        QCOMPARE(hotkeys.key(Hotkey::Results), u"Alt+T"_s);

        shortcuts.change(u"something-else"_s, QKeySequence(u"Alt+S"_s));
        QCOMPARE(changed.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestHotkeys)
#include "tst_hotkeys.moc"
