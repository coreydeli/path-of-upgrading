// SPDX-License-Identifier: MIT
#include "app/itemreader.h"
#include "fakegameinput.h"
#include "itemtexts.h"

#include <QSignalSpy>
#include <QTest>

using namespace pou;

namespace {

const ScreenPoint overItem{u"DP-1"_s, {400, 300}};
const QString currency =
    u"Item Class: Stackable Currency\nRarity: Currency\nDivine Orb\n--------\nStack Size: 1/20"_s;

std::optional<ScreenPoint> mouseOf(const QSignalSpy &spy)
{
    return spy.first().at(1).value<std::optional<ScreenPoint>>();
}

} // namespace

class TestItemReader : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsTheItemUnderTheMouse()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        QSignalSpy read(&reader, &ItemReader::itemRead);
        reader.read();
        QCOMPARE(game.copies, 1);
        QVERIFY(!game.lastAdvanced);
        QVERIFY(reader.isReading());

        Q_EMIT game.itemCopied(itemtexts::helmetNew, overItem);
        QVERIFY(!reader.isReading());
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().at(0).value<Item>().name, u"Doom Visage"_s);
        QCOMPARE(mouseOf(read), std::optional(overItem));
    }

    void advancedCopy()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        reader.setAdvancedCopy(true);
        reader.read();
        QVERIFY(game.lastAdvanced);
    }

    void nudgesTheMouse()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        reader.setAdvancedCopy(true);
        reader.read();
        QVERIFY(!game.lastNudge);
        Q_EMIT game.copyFailed(GameInput::Failure::NothingCopied, {}, overItem);

        // with the advanced copy as well
        reader.read(true);
        QCOMPARE(game.copies, 2);
        QVERIFY(game.lastNudge);
        QVERIFY(game.lastAdvanced);
    }

    void canRead()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        QVERIFY(reader.canRead());
        reader.read();
        QVERIFY(!reader.canRead());
        Q_EMIT game.copyFailed(GameInput::Failure::NothingCopied, {}, overItem);
        QVERIFY(reader.canRead());
        reader.setPaused(true);
        QVERIFY(!reader.canRead());
        reader.setPaused(false);
        QVERIFY(reader.canRead());
    }

    void oneAtATime()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        reader.read();
        reader.read();
        QCOMPARE(game.copies, 1);
        Q_EMIT game.copyFailed(GameInput::Failure::NothingCopied, {}, overItem);
        reader.read();
        QCOMPARE(game.copies, 2);
    }

    void pausedReadsNothing()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        QSignalSpy read(&reader, &ItemReader::itemRead);
        reader.setPaused(true);
        reader.read();
        Q_EMIT game.textCopied(itemtexts::helmetNew);
        QCOMPARE(game.copies, 0);
        QVERIFY(read.isEmpty());

        reader.setPaused(false);
        reader.read();
        QCOMPARE(game.copies, 1);
    }

    void hotkeyReadsWhatIsntGear()
    {
        // for the popup to say it isn't gear
        FakeGameInput game;
        ItemReader reader(&game);
        QSignalSpy read(&reader, &ItemReader::itemRead);
        reader.read();
        Q_EMIT game.itemCopied(currency, std::nullopt);
        QCOMPARE(read.size(), 1);
        QCOMPARE(read.first().at(0).value<Item>().name, u"Divine Orb"_s);
    }

    void notAnItem()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        reader.setHotkey(u"F2"_s);
        QSignalSpy failed(&reader, &ItemReader::failed);
        reader.read();
        Q_EMIT game.itemCopied(u"Hello"_s, overItem);
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().at(0).toString(),
                 u"That wasn't an item, the game copied something else. Hover an item until its tooltip "
                 "shows, then press F2."_s);
        QCOMPARE(mouseOf(failed), std::optional(overItem));
    }

    void whyNothingWasRead_data()
    {
        QTest::addColumn<GameInput::Failure>("why");
        QTest::addColumn<QString>("detail");
        QTest::addColumn<QString>("message");

        using enum GameInput::Failure;
        QTest::newRow("unavailable") << Unavailable << u"The X server has no XTest."_s
                                     << u"The X server has no XTest."_s;
        QTest::newRow("unavailable, unsaid")
            << Unavailable << QString() << u"Items can't be copied out of the game here."_s;
        QTest::newRow("not in front")
            << NotInFront << u"firefox"_s
            << u"Path of Exile 2 wasn't the active window, firefox was, so it didn't copy the item. Click "
               "once in the game, then press F2 again."_s;
        QTest::newRow("not in front, unnamed")
            << NotInFront << QString()
            << u"Path of Exile 2 wasn't the active window, so it didn't copy the item. Click once in the "
               "game, then press F2 again."_s;
        QTest::newRow("key held") << KeyHeld << u"A key stayed down."_s
                                  << u"A key was still down after F2, so the game wasn't asked to copy. "
                                     "Let go of the other keys, then press F2 again."_s;
        QTest::newRow("not over the game")
            << NotOverGame << QString()
            << u"Your mouse wasn't over the game. Hover the item in game, then press F2."_s;
        QTest::newRow("nothing copied")
            << NothingCopied << QString()
            << u"No item under the mouse. Hover an item until its tooltip shows, then press F2."_s;
    }

    void whyNothingWasRead()
    {
        QFETCH(GameInput::Failure, why);
        QFETCH(QString, detail);
        QFETCH(QString, message);

        FakeGameInput game;
        ItemReader reader(&game);
        reader.setHotkey(u"F2"_s);
        QSignalSpy failed(&reader, &ItemReader::failed);
        reader.read();
        Q_EMIT game.copyFailed(why, detail, overItem);
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().at(0).toString(), message);
        QCOMPARE(mouseOf(failed), std::optional(overItem));
    }

    void withoutAHotkey()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        QSignalSpy failed(&reader, &ItemReader::failed);
        reader.read();
        Q_EMIT game.copyFailed(GameInput::Failure::NothingCopied, {}, std::nullopt);
        QVERIFY(failed.first().at(0).toString().endsWith(u"then press the hotkey."));
    }

    void failsRightAway()
    {
        FakeGameInput game;
        game.failRightAway = u"There's no X server."_s;
        ItemReader reader(&game);
        QSignalSpy failed(&reader, &ItemReader::failed);
        QSignalSpy slow(&reader, &ItemReader::slow);
        reader.read();
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().at(0).toString(), u"There's no X server."_s);
        QVERIFY(!reader.isReading());
        QVERIFY(!slow.wait(400));
    }

    void slowGame()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        QSignalSpy slow(&reader, &ItemReader::slow);
        reader.read();
        // not at once, and only the once
        QTest::qWait(50);
        QCOMPARE(slow.size(), 0);
        QVERIFY(slow.wait(1000));
        QVERIFY(reader.isReading());
        QTest::qWait(600);
        QCOMPARE(slow.size(), 1);
    }

    void quickGame()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        QSignalSpy slow(&reader, &ItemReader::slow);
        reader.read();
        Q_EMIT game.itemCopied(itemtexts::helmetNew, std::nullopt);
        QVERIFY(!slow.wait(400));
    }

    void gearYouCopy()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        QSignalSpy read(&reader, &ItemReader::itemRead);
        Q_EMIT game.textCopied(itemtexts::helmetNew);
        QCOMPARE(read.size(), 1);
        QVERIFY(!mouseOf(read));

        // not what you copy to paste somewhere else
        Q_EMIT game.textCopied(currency);
        Q_EMIT game.textCopied(u"Hello"_s);
        QCOMPARE(read.size(), 1);

        reader.setWatchClipboard(false);
        Q_EMIT game.textCopied(itemtexts::helmetNew);
        QCOMPARE(read.size(), 1);
    }

    void notWhileReading()
    {
        FakeGameInput game;
        ItemReader reader(&game);
        QSignalSpy read(&reader, &ItemReader::itemRead);
        reader.read();
        Q_EMIT game.textCopied(itemtexts::helmetNew);
        QVERIFY(read.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestItemReader)
#include "tst_itemreader.moc"
