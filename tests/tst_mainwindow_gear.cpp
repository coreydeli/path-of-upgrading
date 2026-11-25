// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "app/fonts.h"
#include "core/parser.h"
#include "core/sessionguide.h"
#include "core/slots.h"
#include "core/weights.h"
#include "fakegameinput.h"
#include "fakegamewatcher.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "fakepob.h"
#include "fakesecretstore.h"
#include "itemtexts.h"
#include "mainwindowtest.h"
#include "services/datafiles.h"
#include "stylefortests.h"
#include "tradefixtures.h"
#include "updatetexts.h"

#include <KLocalizedQmlContext>
#include <QAccessible>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQuickTest/quicktest.h>
#include <functional>
#include <memory>
#include <optional>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using namespace pou::windowtest;

class TestMainWindowGear : public QObject
{
    Q_OBJECT

public:
    // The window isn't for the screen. Kirigami would ask the desktop portal
    // for the look of the desktop, which starts it.
    static void initMain()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // the app's own controls, as the program has
        useStyleInTests();
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    }

private Q_SLOTS:
    void initTestCase()
    {
        // so the desktop's color settings are neither read nor written
        QStandardPaths::setTestModeEnabled(true);
        // a container can have no fonts at all, and the font pickers need one
        QVERIFY(addBundledFonts());
    }

    void init()
    {
        // Debian 13's Qt 6.8 now and then warns from its own animation timer that the
        // driver isn't running (Arch and Windows never do), so only that one passes there
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        QTest::failOnWarning(
            QRegularExpression(u"^(?!QUnifiedTimer::stopAnimationDriver: driver is not running$)"_s));
#else
        QTest::failOnWarning();
#endif
    }

    static QPointF where(Window &w, const QString &name)
    {
        const auto *item = qobject_cast<QQuickItem *>(w.find(name));
        return item ? item->mapToScene(QPointF(0, 0)) : QPointF(-1, -1);
    }

    // The gear page on a window wide enough for the equipment screen.
    static bool wide(Window &w)
    {
        if (!w.show(u"gearAction"_s) || !w.shown())
            return false;
        w.window->resize(1500, 900);
        return QTest::qWaitFor([&] { return w.settled() && isVisible(w.find(u"equipment"_s)); }, 2000);
    }

    void equipmentScreen()
    {
        Window w;
        QVERIFY(wide(w));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)},
                {u"Ring 1"_s, item(itemtexts::advancedRing)},
                {u"Jewel 2"_s, item(jewel)}},
               {u"Ring 1"_s});
        QVERIFY(w.settled());

        // the rest of the page stays
        QVERIFY(isVisible(w.find(u"recommendedUpgrades"_s)));
        QVERIFY(isVisible(w.find(u"resistances"_s)));
        QVERIFY(isVisible(w.find(u"overallScore"_s)));
        QVERIFY(isVisible(w.find(u"gearHelp"_s)));

        // the helmet at the top with the amulet beside it, the weapons either side of
        // the body armour, the gloves and a ring below, the belt, the boots
        const auto at = [&](const char *slot) {
            return where(w, u"tile:"_s + QString::fromLatin1(slot));
        };
        QCOMPARE(at("Helmet").x(), at("Body Armour").x());
        QVERIFY(at("Helmet").y() < at("Body Armour").y());
        QVERIFY(at("Amulet").x() > at("Helmet").x());
        QCOMPARE(at("Amulet").y(), at("Helmet").y());
        QVERIFY(at("Weapon").x() < at("Body Armour").x());
        QCOMPARE(at("Weapon").y(), at("Body Armour").y());
        QVERIFY(at("Offhand").x() > at("Body Armour").x());
        QCOMPARE(at("Offhand").y(), at("Body Armour").y());
        QCOMPARE(at("Gloves").x(), at("Weapon").x());
        QVERIFY(at("Gloves").y() > at("Weapon").y());
        QCOMPARE(at("Belt").x(), at("Body Armour").x());
        QCOMPARE(at("Belt").y(), at("Gloves").y());
        QCOMPARE(at("Ring 1").x(), at("Offhand").x());
        QCOMPARE(at("Ring 1").y(), at("Gloves").y());
        QCOMPARE(at("Boots").x(), at("Belt").x());
        QVERIFY(at("Boots").y() > at("Belt").y());
        QCOMPARE(at("Ring 2").x(), at("Ring 1").x());
        QCOMPARE(at("Ring 2").y(), at("Boots").y());
        // the slots that aren't on the screen are in a row underneath
        QVERIFY(at("Jewel 2").y() > at("Boots").y());
        QVERIFY(isVisible(w.find(u"equipmentOthers"_s)));

        // what a worn slot says
        QObject *helmet = w.find(u"tile:Helmet"_s);
        QCOMPARE(text(helmet->findChild<QObject *>(u"slotName"_s)), u"Helmet"_s);
        QCOMPARE(text(helmet->findChild<QObject *>(u"itemName"_s)), u"Grim Crown"_s);
        QCOMPARE(helmet->property("description").toString(), u"Grim Crown, Visored Helm, item level 50"_s);
        QCOMPARE(spoken(helmet), u"Helmet"_s);
        QVERIFY(isVisible(helmet->findChild<QObject *>(u"tileGauge"_s)));
        QCOMPARE(text(helmet->findChild<QObject *>(u"tileItemLevel"_s)), u"Item level 50"_s);
        QVERIFY(isVisible(helmet->findChild<QObject *>(u"tileItemLevel"_s)));
        QCOMPARE(w.find(u"tile:Ring 1"_s)->property("description").toString(),
                 u"Storm Loop, Sapphire Ring, item level 75, from Path of Building"_s);

        // and an empty one is a dashed frame that says so, with no score
        QObject *gloves = w.find(u"tile:Gloves"_s);
        QVERIFY(gloves->property("empty").toBool());
        QCOMPARE(text(gloves->findChild<QObject *>(u"itemName"_s)), u"Empty"_s);
        QCOMPARE(gloves->property("description").toString(), u"Nothing saved yet"_s);
        QVERIFY(!isVisible(gloves->findChild<QObject *>(u"tileGauge"_s)));
        QVERIFY(!isVisible(gloves->findChild<QObject *>(u"tileItemLevel"_s)));
        QVERIFY(!helmet->property("empty").toBool());

        // each slot once
        for (const char *slot : {"Weapon", "Offhand", "Helmet", "Body Armour", "Gloves", "Boots", "Belt",
                                 "Amulet", "Ring 1", "Ring 2", "Jewel 2"})
            QCOMPARE(findAll(w.page(), u"tile:"_s + QString::fromLatin1(slot)).size(), 1);
    }

    void equipmentTabOrder()
    {
        Window w;
        QVERIFY(wide(w));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.settled());
        // across the screen and then down it
        const QStringList reading{u"Helmet"_s, u"Amulet"_s, u"Weapon"_s, u"Body Armour"_s, u"Offhand"_s,
                                  u"Gloves"_s, u"Belt"_s,   u"Ring 1"_s, u"Boots"_s,       u"Ring 2"_s};
        auto *tile = qobject_cast<QQuickItem *>(w.find(u"tile:"_s + reading.first()));
        QVERIFY(tile);
        QVERIFY(tile->activeFocusOnTab());
        for (qsizetype i = 1; i < reading.size(); ++i) {
            tile = tile->nextItemInFocusChain(true);
            QVERIFY(tile);
            QCOMPARE(tile->objectName(), u"tile:"_s + reading.at(i));
        }
    }

    void equipmentPanel()
    {
        Window w;
        QVERIFY(wide(w));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.settled());
        const RecommendedUpgrades *upgrades = w.app->recommendedUpgrades();

        // nothing picked, so it says to
        QVERIFY(isVisible(w.find(u"panelHint"_s)));
        QCOMPARE(text(w.find(u"panelHint"_s)), u"Pick a slot to see what would improve it."_s);
        QVERIFY(findAll(w.page(), u"panelContent"_s).isEmpty());

        // a worn slot shows its notes, what to look for and a search
        QVERIFY(click(w.find(u"tile:Helmet"_s)));
        QCOMPARE(upgrades->chosen(), u"Helmet"_s);
        QVERIFY(w.settled());
        QVERIFY(!isVisible(w.find(u"panelHint"_s)));
        QCOMPARE(text(w.find(u"panelSlot"_s)), u"Helmet"_s);
        QCOMPARE(text(w.find(u"panelItem"_s)), u"Grim Crown"_s);
        QCOMPARE(text(w.find(u"panelBase"_s)), u"Visored Helm, item level 50, no quality"_s);
        QVERIFY(isVisible(w.find(u"panelGauge"_s)));
        // its mods, which a plain copy says the kind of from the table
        QVERIFY(isVisible(w.find(u"panelMods"_s)));
        QCOMPARE(findAll(w.find(u"panelMods"_s), u"modText"_s).size(), 3);
        QVERIFY(!isVisible(w.find(u"panelMarks"_s)));
        const auto notes = w.app->gearList()->index(2).data(GearList::NotesRole).value<QList<GearNote>>();
        QVERIFY(!notes.isEmpty());
        const QList<QObject *> labels = findAll(w.find(u"panelNotes"_s), u"noteLabel"_s);
        QCOMPARE(labels.size(), notes.size());
        QCOMPARE(text(labels.first()), notes.first().label);
        const QString lookFor = text(w.find(u"panelLookFor"_s));
        QVERIFY(lookFor.contains(u" or more"_s));
        QCOMPARE(lookFor, upgrades->chosenLookFor());
        QCOMPARE(findAll(w.find(u"panelLookFor"_s), u"statChip"_s).size(),
                 upgrades->chosenLookForStats().size());
        QCOMPARE(findAll(w.find(u"panelCanRoll"_s), u"statChip"_s).size(), upgrades->chosenCanRoll().size());
        QVERIFY(isVisible(w.find(u"panelCanRoll"_s)));
        QVERIFY(text(w.find(u"panelCanRoll"_s)).contains(u" at item level "_s));
        QCOMPARE(text(w.find(u"panelCanRoll"_s)), upgrades->chosenCanRoll().join(u'\n'));
        // and each says prefix or suffix, to those who listen too
        QCOMPARE(w.find(u"panelLookFor"_s)->property("kinds").toStringList(), upgrades->chosenLookForKinds());
        QCOMPARE(w.find(u"panelCanRoll"_s)->property("kinds").toStringList(), upgrades->chosenCanRollKinds());
        QVERIFY(spoken(w.find(u"panelLookFor"_s)).contains(u", prefix"_s) ||
                spoken(w.find(u"panelLookFor"_s)).contains(u", suffix"_s));
        QVERIFY(spoken(w.find(u"panelCanRoll"_s)).contains(u" at item level "_s));
        QCOMPARE(findAll(w.find(u"panelCanRoll"_s), u"statChip"_s).first()->property("kind").toString(),
                 upgrades->chosenCanRollKinds().first());
        QCOMPARE(w.find(u"panelLookForLabel"_s)->property("help").toString(), upgrades->lookForHelp());
        QCOMPARE(w.find(u"panelCanRollLabel"_s)->property("help").toString(), upgrades->canRollHelp());
        QVERIFY(w.find(u"tile:Helmet"_s)->property("chosen").toBool());
        QVERIFY(!w.find(u"tile:Gloves"_s)->property("chosen").toBool());

        QObject *find = w.find(u"panelFind"_s);
        QVERIFY(isVisible(find));
        QVERIFY(!isVisible(w.find(u"panelTradeStatus"_s)));
        w.network->answers = {{.never = true}};
        QVERIFY(click(find));
        QVERIFY(isVisible(w.find(u"panelTradeStatus"_s)));
        QCOMPARE(text(w.in(u"panelTradeStatus"_s, u"tradeStatusText"_s)), u"Searching the trade site"_s);
        QVERIFY(!find->property("enabled").toBool());

        // another slot takes its place
        QVERIFY(click(w.find(u"tile:Boots"_s)));
        QTRY_COMPARE(text(w.find(u"panelSlot"_s)), u"Boots"_s);
        QCOMPARE(findAll(w.page(), u"panelContent"_s).size(), 1);
        QVERIFY(!isVisible(w.find(u"panelTradeStatus"_s)));
        QVERIFY(w.find(u"tile:Boots"_s)->property("chosen").toBool());
        QVERIFY(!w.find(u"tile:Helmet"_s)->property("chosen").toBool());
    }

    void equipmentShowsMods()
    {
        Window w;
        QVERIFY(wide(w));
        w.play({{u"Gloves"_s, item(u"Item Class: Gloves\nRarity: Rare\nDoom Vise\nSorcerer Gloves\n--------\n"
                                   "Quality: +20% (augmented)\nEnergy Shield: 280\n--------\n"
                                   "Item Level: 81\n--------\n"
                                   "{ Suffix Modifier \"of the Squall\" (Tier: 3) }\n"
                                   "+39(36-40)% to Lightning Resistance\n"
                                   "{ Prefix Modifier \"Hale\" (Tier: 3) }\n"
                                   "+33(33-41) to maximum Life (fractured)\n"
                                   "--------\nCorrupted"_s)},
                {u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.settled());

        // the tile marks it, and a plain one has no mark
        QCOMPARE(text(w.find(u"tile:Gloves"_s)->findChild<QObject *>(u"tileMarks"_s)),
                 u"Fractured, Corrupted"_s);
        QVERIFY(!isVisible(w.find(u"tile:Helmet"_s)->findChild<QObject *>(u"tileMarks"_s)));
        QVERIFY(w.find(u"tile:Gloves"_s)
                    ->property("description")
                    .toString()
                    .endsWith(u", fractured, corrupted"_s));

        QVERIFY(click(w.find(u"tile:Gloves"_s)));
        QVERIFY(w.settled());
        QCOMPARE(text(w.find(u"panelBase"_s)), u"Sorcerer Gloves, item level 81, quality 20%"_s);
        QCOMPARE(text(w.find(u"panelMarks"_s)), u"Fractured, Corrupted"_s);

        // the game's order, each under its heading, the fractured one marked
        QVERIFY(isVisible(w.find(u"panelMods"_s)));
        QStringList groups;
        for (QObject *group : findAll(w.find(u"panelMods"_s), u"modGroup"_s))
            groups << text(group);
        QCOMPARE(groups, (QStringList{u"Prefixes"_s, u"Suffixes"_s}));
        const QList<QObject *> mods = findAll(w.find(u"panelMods"_s), u"modText"_s);
        QCOMPARE(mods.size(), 2);
        QCOMPARE(text(mods.at(0)), u"+33 to maximum Life"_s);
        QCOMPARE(text(mods.at(1)), u"+39% to Lightning Resistance"_s);
        QCOMPARE(findAll(w.find(u"panelMods"_s), u"modFractured"_s).size(), 2);
        QVERIFY(isVisible(findAll(w.find(u"panelMods"_s), u"modFractured"_s).first()));
        QVERIFY(!isVisible(findAll(w.find(u"panelMods"_s), u"modFractured"_s).last()));

        // and an empty slot shows none of it
        QVERIFY(click(w.find(u"tile:Boots"_s)));
        QTRY_COMPARE(text(w.find(u"panelSlot"_s)), u"Boots"_s);
        QVERIFY(!isVisible(w.find(u"panelMods"_s)));
        QVERIFY(!isVisible(w.find(u"panelMarks"_s)));
    }

    // Whether the slot panel's frame goes round all it shows, down to its
    // last chip, and the board round the panel.
    static bool holdsItsContent(Window &w)
    {
        auto *panel = qobject_cast<QQuickItem *>(w.find(u"slotPanel"_s));
        auto *board = qobject_cast<QQuickItem *>(w.find(u"equipment"_s));
        if (!panel || !board)
            return false;
        const QList<QObject *> shown = findAll(panel, u"panelContent"_s) + findAll(panel, u"statChip"_s);
        for (QObject *o : shown) {
            auto *item = qobject_cast<QQuickItem *>(o);
            if (item && item->isVisible() &&
                item->mapToItem(panel, QPointF(0, item->height())).y() > panel->height())
                return false;
        }
        return !shown.isEmpty() &&
               panel->mapToItem(board, QPointF(0, panel->height())).y() <= board->height();
    }

    void equipmentPanelHoldsItsContent()
    {
        Window w;
        QVERIFY(wide(w));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.settled());
        // a short slot first, so the taller ones after it have to grow the panel
        for (const QString &slot : {u"Weapon"_s, u"Helmet"_s, u"Gloves"_s, u"Helmet"_s}) {
            QVERIFY(click(w.find(u"tile:"_s + slot)));
            QTRY_COMPARE(text(w.find(u"panelSlot"_s)), slot);
            QVERIFY(w.settled());
            QVERIFY2(holdsItsContent(w), qPrintable(slot));
        }
    }

    void uniqueSlotNotes()
    {
        const Item belt = item(u"Item Class: Belts\nRarity: Unique\nCoil of Ash\nRawhide Belt\n--------\n+40 "
                               u"to maximum Life"_s);
        // the slot list
        {
            Window w;
            QVERIFY(w.show(u"gearAction"_s));
            w.play({{u"Belt"_s, belt}});
            QObject *slot = w.find(u"slot:Belt"_s);
            const QList<QObject *> labels = findAll(slot, u"noteLabel"_s);
            QCOMPARE(labels.size(), 2);
            QCOMPARE(text(labels.at(0)), u"Unique"_s);
            QCOMPARE(text(labels.at(1)), u"Judged by"_s);
            // chips, as every other slot's notes are
            QVERIFY(findAll(slot, u"noteSentence"_s).isEmpty());
            QVERIFY(findAll(slot, u"statChip"_s).size() >= 2);
        }
        // the slot panel
        {
            Window w;
            QVERIFY(wide(w));
            w.play({{u"Belt"_s, belt}});
            QVERIFY(w.settled());
            QVERIFY(click(w.find(u"tile:Belt"_s)));
            QVERIFY(w.settled());
            const QList<QObject *> labels = findAll(w.find(u"panelNotes"_s), u"noteLabel"_s);
            QCOMPARE(labels.size(), 2);
            QCOMPARE(text(labels.first()), u"Unique"_s);
            QVERIFY(!findAll(w.find(u"panelNotes"_s), u"statChip"_s).isEmpty());
        }
    }

    void equipmentPanelEmptySlot()
    {
        Window w;
        QVERIFY(wide(w));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.settled());
        QVERIFY(click(w.find(u"tile:Gloves"_s)));
        QVERIFY(w.settled());
        QCOMPARE(text(w.find(u"panelSlot"_s)), u"Gloves"_s);
        QCOMPARE(text(w.find(u"panelItem"_s)), u"Nothing saved yet"_s);
        QVERIFY(!isVisible(w.find(u"panelBase"_s)));
        QVERIFY(!isVisible(w.find(u"panelGauge"_s)));
        QCOMPARE(text(w.in(u"panelNotes"_s, u"noteLabel"_s)), u"Your build wants"_s);
        QVERIFY(text(w.find(u"panelLookFor"_s)).contains(u" or more"_s));
        QVERIFY(isVisible(w.find(u"panelCanRoll"_s)));
        // the weapon has no item class to go by
        QVERIFY(click(w.find(u"tile:Weapon"_s)));
        QVERIFY(w.settled());
        QCOMPARE(text(w.find(u"panelSlot"_s)), u"Weapon"_s);
        QVERIFY(!isVisible(w.find(u"panelCanRoll"_s)));
        QVERIFY(click(w.find(u"tile:Gloves"_s)));
        QVERIFY(w.settled());
        // there's nothing worn to search for a better one of
        QVERIFY(!isVisible(w.find(u"panelFind"_s)));
        QVERIFY(!isVisible(w.find(u"panelTakeOff"_s)));

        // taking off what's worn leaves the slot picked, and empty
        QVERIFY(click(w.find(u"tile:Helmet"_s)));
        QVERIFY(w.settled());
        QVERIFY(click(w.find(u"panelTakeOff"_s)));
        QVERIFY(w.settled());
        QVERIFY(w.app->character().character().gear.isEmpty());
        QCOMPARE(text(w.find(u"panelSlot"_s)), u"Helmet"_s);
        QCOMPARE(text(w.find(u"panelItem"_s)), u"Nothing saved yet"_s);
        QVERIFY(w.find(u"tile:Helmet"_s)->property("empty").toBool());
    }

    void takeOffGear()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QObject *takeOff = w.find(u"slot:Helmet"_s)->findChild<QObject *>(u"takeOffButton"_s);
        QCOMPARE(spoken(takeOff), u"Unequip Helmet"_s);
        QVERIFY(click(takeOff));
        QVERIFY(w.app->character().character().gear.isEmpty());
        QCOMPARE(w.find(u"slot:Helmet"_s)->property("description").toString(), u"Nothing saved yet"_s);
    }

    void cantTakeOff()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        const QString id = w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        const QString file = w.dir.filePath(u"data/characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QVERIFY(click(w.find(u"slot:Helmet"_s)->findChild<QObject *>(u"takeOffButton"_s)));
        QObject *error = w.find(u"gearError"_s);
        QVERIFY(isVisible(error));
        QVERIFY(text(error).startsWith(u"Couldn't save Wren: "_s));
    }

    void resistancesNotKnown()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        w.play({});
        QVERIFY(isVisible(w.find(u"resistances"_s)));
        QCOMPARE(w.find(u"resistancesHeader"_s)->property("title").toString(), u"Resistances"_s);
        QCOMPARE(text(w.find(u"resistSource"_s)),
                 u"You don't have any yet. Import a build, or type them in from your character sheet."_s);
        QVERIFY(!isVisible(w.find(u"resistSummary"_s)));
        QVERIFY(!w.find(u"fromSheetSwitch"_s)->property("checked").toBool());
        QVERIFY(!isVisible(w.find(u"fireResist"_s)));
        QVERIFY(!isVisible(w.find(u"fireMaximum"_s)));
        QVERIFY(!isVisible(w.find(u"resistMargin"_s)));
        QVERIFY(!isVisible(w.find(u"resistError"_s)));
    }

    void resistancesFromTheBuild()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        w.play({});
        PerElement<std::optional<double>> values;
        values[Element::Fire] = 80;
        values[Element::Cold] = 60;
        QVERIFY(w.app->character().change([&values](Character &c) {
            c.resists = snapshotResists({.values = values, .cap = {}, .buffer = {}}, {}, ResistSource::Pob);
        }));
        QObject *summary = w.find(u"resistSummary"_s);
        QCOMPARE(text(summary), u"Fire 80%, Cold 60%"_s);
        QCOMPARE(text(w.find(u"resistSource"_s)),
                 u"These come from your build in Path of Building, plus what you have equipped now."_s);
        QVERIFY(!isVisible(w.find(u"fireResist"_s)));
        for (const QString &field :
             {u"fireMaximum"_s, u"coldMaximum"_s, u"lightningMaximum"_s, u"chaosMaximum"_s}) {
            QVERIFY(isVisible(w.find(field)));
            QCOMPARE(w.spinBox(field)->property("value").toInt(), 75);
        }
        QCOMPARE(w.spinBox(u"resistMargin"_s)->property("value").toInt(), 15);
        QObject *buffer = w.find(u"resistMargin"_s);
        QCOMPARE(buffer->property("label").toString(), u"Safety buffer"_s);
        QVERIFY(buffer->property("description").toString().startsWith(u"How far over the maximum"_s));
    }

    void typeInResistances()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        w.play({});
        Resistances *resists = w.app->resistances();
        QVERIFY(toggle(w.find(u"fromSheetSwitch"_s)));
        QVERIFY(resists->isFromSheet());
        QVERIFY(!isVisible(w.find(u"resistSummary"_s)));
        QCOMPARE(
            text(w.find(u"resistSource"_s)),
            u"You typed these in from your character sheet, and they follow what you have equipped now."_s);
        for (const QString &field :
             {u"fireResist"_s, u"coldResist"_s, u"lightningResist"_s, u"chaosResist"_s})
            QVERIFY(isVisible(w.find(field)));

        QVERIFY(setValue(w.spinBox(u"fireResist"_s), 76, "valueModified"));
        QVERIFY(setValue(w.spinBox(u"coldResist"_s), 120, "valueModified"));
        QVERIFY(setValue(w.spinBox(u"lightningResist"_s), 30, "valueModified"));
        QVERIFY(setValue(w.spinBox(u"chaosResist"_s), -20, "valueModified"));
        QCOMPARE(resists->summary(), u"Fire 76%, Cold 120%, Lightning 30%, Chaos -20%"_s);
        QVERIFY(setValue(w.spinBox(u"fireMaximum"_s), 78, "valueModified"));
        QVERIFY(setValue(w.spinBox(u"chaosMaximum"_s), 70, "valueModified"));
        QVERIFY(setValue(w.spinBox(u"resistMargin"_s), 2, "valueModified"));
        QCOMPARE(resists->fireMaximum(), 78.0);
        QCOMPARE(resists->coldMaximum(), 75.0);
        QCOMPARE(resists->chaosMaximum(), 70.0);
        QCOMPARE(resists->margin(), 2.0);
        // no sheet goes that high
        QVERIFY(setValue(w.spinBox(u"fireResist"_s), 900, "valueModified"));
        QCOMPARE(resists->fire(), 500.0);

        // and they show what it has
        resists->setFire(50);
        resists->setCold(40);
        resists->setLightning(45);
        resists->setChaos(-5);
        resists->setLightningMaximum(77);
        resists->setMargin(4);
        QCOMPARE(w.spinBox(u"fireResist"_s)->property("value").toInt(), 50);
        QCOMPARE(w.spinBox(u"coldResist"_s)->property("value").toInt(), 40);
        QCOMPARE(w.spinBox(u"lightningResist"_s)->property("value").toInt(), 45);
        QCOMPARE(w.spinBox(u"chaosResist"_s)->property("value").toInt(), -5);
        QCOMPARE(w.spinBox(u"lightningMaximum"_s)->property("value").toInt(), 77);
        QCOMPARE(w.spinBox(u"resistMargin"_s)->property("value").toInt(), 4);

        resists->setFromSheet(false);
        QVERIFY(!w.find(u"fromSheetSwitch"_s)->property("checked").toBool());
        QVERIFY(!isVisible(w.find(u"fireResist"_s)));
        QVERIFY(!isVisible(w.find(u"resistSummary"_s)));
        QCOMPARE(text(w.find(u"resistSource"_s)),
                 u"You don't have any yet. Import a build, or type them in from your character sheet."_s);
    }

    void cantSaveResistances()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        const QString id = w.play({});
        const QString file = w.dir.filePath(u"data/characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QVERIFY(toggle(w.find(u"fromSheetSwitch"_s)));
        QObject *error = w.find(u"resistError"_s);
        QVERIFY(isVisible(error));
        QVERIFY(text(error).startsWith(u"Couldn't save Wren: "_s));
    }

    void gearScoreForNobody()
    {
        Window w;
        QVERIFY(w.show(u"gearScoreAction"_s));
        QVERIFY(isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(!isVisible(w.find(u"scoreError"_s)));
        // the default start can be changed all the same
        QVERIFY(isVisible(w.find(u"scoreState"_s)));
        QVERIFY(isVisible(w.find(u"scoreHelp"_s)));
        QVERIFY(isVisible(w.in(u"stats"_s, u"stat:Life"_s)));
        QVERIFY(toggle(w.find(u"weightsOnlySwitch"_s)));
        QVERIFY(!isVisible(w.find(u"nobodyText"_s)));
        QCOMPARE(w.app->character().character().name, u"My character"_s);
        QVERIFY(w.app->character().character().weightsOnly);
    }

    void gearScore()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"gearScoreAction"_s));
        w.play({});
        QVERIFY(!isVisible(w.find(u"nobodyText"_s)));
        QObject *heading = search(w.page(), [](QObject *o) {
            return o->inherits("QQuickItem") && o->property("title").toString() == u"For Wren";
        });
        QVERIFY(isVisible(heading));

        QObject *state = w.find(u"scoreState"_s);
        QVERIFY(isVisible(state));
        QCOMPARE(text(state), u"A rough guide until Path of Building has weighed your build."_s);
        QCOMPARE(state->property("description").toString(), w.app->gearScoring()->summary());
        QVERIFY(isVisible(w.find(u"weighAgainButton"_s)));
        // with Path of Building off here, there is nothing to ask
        QVERIFY(!w.find(u"weighAgainButton"_s)->property("enabled").toBool());
        QVERIFY(isVisible(w.find(u"scoreHelp"_s)));
        QVERIFY(isVisible(w.find(u"stats"_s)));
        QObject *impact = search(w.page(), [](QObject *o) {
            return o->inherits("QQuickItem") &&
                   o->property("title").toString() == u"How much each stat counts";
        });
        QVERIFY(isVisible(impact));
        QVERIFY(isVisible(w.find(u"impactHelp"_s)));

        QObject *life = w.in(u"stats"_s, u"stat:Life"_s);
        QCOMPARE(text(life), u"Life"_s);
        QCOMPARE(life->property("description").toString(), u"Counts in every slot but Weapon and Jewel."_s);
        QObject *level = life->findChild<QObject *>(u"levelBox"_s);
        QVERIFY(isVisible(level));
        QCOMPARE(level->property("currentText").toString(), u"Normal (1x)"_s);
        QCOMPARE(level->property("model").toStringList().at(0), u"Not at all (0x)"_s);
        QCOMPARE(w.find(u"resetButton"_s)->property("enabled").toBool(), false);
    }

    void statLevels()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"gearScoreAction"_s));
        w.play({});
        QObject *life = w.in(u"stats"_s, u"stat:Life"_s)->findChild<QObject *>(u"levelBox"_s);
        QVERIFY(activate(life, 4));
        QCOMPARE(w.app->character().character().tweaks, ProfileTweaks({{u"Life"_s, 2.0}}));
        QCOMPARE(life->property("currentText").toString(), u"Double (2x)"_s);

        QObject *reset = w.find(u"resetButton"_s);
        QCOMPARE(reset->property("enabled").toBool(), true);
        QVERIFY(click(reset));
        QCOMPARE(w.app->character().character().tweaks, ProfileTweaks());
        QCOMPARE(life->property("currentText").toString(), u"Normal (1x)"_s);
        QCOMPARE(reset->property("enabled").toBool(), false);
    }

    void judgeByWeightsAlone()
    {
        Window w;
        QVERIFY(w.show(u"gearScoreAction"_s));
        w.play({});
        QObject *sw = w.find(u"weightsOnlySwitch"_s);
        QVERIFY(isVisible(sw));
        QVERIFY(!sw->property("checked").toBool());
        QVERIFY(toggle(sw));
        QVERIFY(w.app->character().character().weightsOnly);
        QCOMPARE(text(w.find(u"scoreState"_s)), u"Judged by your stat weights alone"_s);
        QCOMPARE(w.find(u"scoreState"_s)->property("description").toString(),
                 u"Items are judged by your stat weights alone, with no gear score."_s);
        QVERIFY(!isVisible(w.find(u"stats"_s)));
        QVERIFY(!isVisible(w.find(u"scoreHelp"_s)));

        // and back
        QVERIFY(toggle(sw));
        QVERIFY(!w.app->character().character().weightsOnly);
        QVERIFY(isVisible(w.in(u"stats"_s, u"stat:Life"_s)));
    }

    void cantSaveGearScore()
    {
        Window w;
        QVERIFY(w.show(u"gearScoreAction"_s));
        const QString id = w.play({});
        const QString file = w.dir.filePath(u"data/characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QVERIFY(toggle(w.find(u"weightsOnlySwitch"_s)));
        QObject *error = w.find(u"scoreError"_s);
        QVERIFY(isVisible(error));
        QVERIFY(text(error).startsWith(u"Couldn't save Wren: "_s));
    }

    void weightsForNobody()
    {
        Window w;
        QVERIFY(w.show(u"weightsAction"_s));
        QVERIFY(isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(!isVisible(w.find(u"noWeightsText"_s)));
        QVERIFY(!isVisible(w.find(u"weightsError"_s)));
        QVERIFY(!isVisible(w.find(u"undoMessage"_s)));
        // the default weights can be changed all the same
        QVERIFY(isVisible(w.find(u"addStatButton"_s)));
        QVERIFY(isVisible(w.find(u"suggestButton"_s)));
        QVERIFY(isVisible(w.find(u"weightsHelp"_s)));
        QVERIFY(isVisible(w.find(u"resetButton"_s)));
        QVERIFY(!w.find(u"resetButton"_s)->property("enabled").toBool());
        QVERIFY(w.app->statWeights()->rowCount() > 0);
    }

    void weights()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QVERIFY(!isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(!isVisible(w.find(u"noWeightsText"_s)));
        QVERIFY(isVisible(w.find(u"weightsHelp"_s)));
        QObject *heading = search(w.page(), [](QObject *o) {
            return o->inherits("QQuickItem") && o->property("title").toString() == u"For Wren";
        });
        QVERIFY(isVisible(heading));

        QObject *armour = w.find(u"weight:[Base] Armour"_s);
        QVERIFY(isVisible(armour));
        QCOMPARE(text(armour), u"Armour"_s);
        QCOMPARE(armour->property("description").toString(), u"The item's own, mods included"_s);
        QObject *box = armour->findChild<QObject *>(u"weightBox"_s);
        QCOMPARE(box->property("displayText").toString(), QLocale().toString(0.05, 'f', 2));

        QObject *life = w.find(u"weight:# to maximum Life"_s);
        QCOMPARE(text(life), u"# to maximum Life"_s);
        QVERIFY(!isVisible(life->findChild<QObject *>(u"descriptionText"_s)));
        QCOMPARE(life->findChild<QObject *>(u"weightBox"_s)->property("displayText").toString(),
                 QLocale().toString(1));
        // with no zeros after it
        QCOMPARE(w.find(u"weight:#% to Fire Resistance"_s)
                     ->findChild<QObject *>(u"weightBox"_s)
                     ->property("displayText")
                     .toString(),
                 QLocale().toString(1.5, 'f', 1));
        QVERIFY(!w.find(u"resetButton"_s)->property("enabled").toBool());
        QVERIFY(!isVisible(w.find(u"undoMessage"_s)));
    }

    void editWeight()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QObject *life = w.find(u"weight:# to maximum Life"_s)->findChild<QObject *>(u"weightBox"_s);
        // in thousandths
        QVERIFY(setValue(life, 2500, "valueModified"));
        QCOMPARE(w.app->character().character().weights.value(u"# to maximum Life"_s), 2.5);
        QCOMPARE(life->property("displayText").toString(), QLocale().toString(2.5, 'f', 1));
        QCOMPARE(typed(life, QLocale().toString(0.125, 'f', 3)), 125);
        QCOMPARE(typed(life, QLocale().toString(-2)), -2000);
        QCOMPARE(life->property("stepSize").toInt(), 100);
        QVERIFY(w.find(u"resetButton"_s)->property("enabled").toBool());
        // as you can change it back yourself
        QVERIFY(!isVisible(w.find(u"undoMessage"_s)));
    }

    void removeWeight()
    {
        Window w;
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QVERIFY(click(w.find(u"weight:# to maximum Mana"_s)->findChild<QObject *>(u"removeButton"_s)));
        QVERIFY(!w.app->character().character().weights.contains(u"# to maximum Mana"_s));
        QTRY_VERIFY(
            !search(w.page(), [](QObject *o) { return o->objectName() == u"weight:# to maximum Mana"; }));
        QVERIFY(isVisible(w.find(u"weight:# to maximum Life"_s)));
    }

    void noWeights()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        for (const QString &key : defaultWeights().keys())
            w.app->statWeights()->remove(key);
        QVERIFY(isVisible(w.find(u"noWeightsText"_s)));
        QVERIFY(w.find(u"resetButton"_s)->property("enabled").toBool());
    }

    void resetWeights()
    {
        Window w;
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QObject *life = w.find(u"weight:# to maximum Life"_s)->findChild<QObject *>(u"weightBox"_s);
        QVERIFY(setValue(life, 2000, "valueModified"));
        QObject *reset = w.find(u"resetButton"_s);
        QVERIFY(click(reset));
        QCOMPARE(w.app->character().character().weights, defaultWeights());
        QCOMPARE(life->property("displayText").toString(), QLocale().toString(1));
        QVERIFY(!reset->property("enabled").toBool());

        QObject *message = w.find(u"undoMessage"_s);
        QVERIFY(isVisible(message));
        QCOMPARE(text(message), u"Your weights are back to the defaults."_s);
        QVERIFY(QMetaObject::invokeMethod(w.find(u"undoAction"_s), "trigger"));
        QCOMPARE(w.app->character().character().weights.value(u"# to maximum Life"_s), 2.0);
        QCOMPARE(life->property("displayText").toString(), QLocale().toString(2));
        QVERIFY(!isVisible(message));
    }

    void addWeight()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        // once the trade site has said what stats there are
        QVERIFY(w.rated());
        QVERIFY(click(w.find(u"addStatButton"_s)));
        QObject *dialog = w.find(u"statPicker"_s);
        QVERIFY(isVisible(dialog));
        QVERIFY(!isVisible(w.find(u"pickerMessage"_s)));
        QVERIFY(!isVisible(w.find(u"noStats"_s)));

        QVERIFY(w.find(u"statSearch"_s)->setProperty("text", u"increased"_s));
        QVERIFY(w.settled());
        QObject *speed = w.find(u"stat:#% increased Movement Speed"_s);
        QVERIFY(!speed->property("enabled").toBool());
        QCOMPARE(text(speed->findChild<QObject *>(u"statNote"_s)), u"It has a weight already"_s);
        QObject *armour = w.find(u"stat:#% increased Armour"_s);
        QVERIFY(armour->property("enabled").toBool());
        QCOMPARE(text(armour->findChild<QObject *>(u"statNote"_s)), u"Explicit"_s);

        QVERIFY(click(armour));
        QCOMPARE(w.app->character().character().weights.value(u"#% increased Armour"_s), 1.0);
        QTRY_VERIFY(!isVisible(dialog));
        QVERIFY(isVisible(w.find(u"weight:#% increased Armour"_s)));
    }

    void addWeightByKeys()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QVERIFY(w.rated());
        QVERIFY(click(w.find(u"addStatButton"_s)));
        QObject *dialog = w.find(u"statPicker"_s);
        QObject *search = w.find(u"statSearch"_s);
        // Armour first, which has a weight already
        QVERIFY(search->setProperty("text", u"armour"_s));
        QVERIFY(w.settled());
        const auto weight = [&w](const QString &key) {
            return w.app->character().character().weights.value(key, -1);
        };
        QVERIFY(press(search, Qt::Key_Return));
        QVERIFY(press(search, Qt::Key_Down));
        QVERIFY(press(search, Qt::Key_Up));
        QVERIFY(press(search, Qt::Key_Return));
        QCOMPARE(weight(u"#% increased Armour"_s), -1.0);
        QVERIFY(isVisible(dialog));

        QVERIFY(press(search, Qt::Key_Down));
        QVERIFY(press(search, Qt::Key_Return));
        QCOMPARE(weight(u"#% increased Armour"_s), 1.0);
        QTRY_VERIFY(!isVisible(dialog));

        // and searching again from the start
        QVERIFY(click(w.find(u"addStatButton"_s)));
        QCOMPARE(text(search), QString());
        QCOMPARE(w.app->statPicker()->query(), QString());
        QVERIFY(search->setProperty("text", u"block"_s));
        QVERIFY(w.settled());
        QVERIFY(press(search, Qt::Key_Enter));
        QCOMPARE(weight(u"[Base] Block Chance"_s), 1.0);
    }

    void noStatFound()
    {
        Window w;
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QVERIFY(click(w.find(u"addStatButton"_s)));
        QVERIFY(w.find(u"statSearch"_s)->setProperty("text", u"fire life"_s));
        QVERIFY(isVisible(w.find(u"noStats"_s)));
    }

    void pickerSaysWhereTheStatsComeFrom()
    {
        // with none kept, it asks nothing as it opens, and has them once the
        // Refresh button on the Trade page has asked
        Window w(true, false, {}, used(), true, nullptr, false);
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QVERIFY(click(w.find(u"addStatButton"_s)));
        QObject *message = w.find(u"pickerMessage"_s);
        QVERIFY(isVisible(message));
        QCOMPARE(text(message),
                 u"You get the trade site's list of stats with your first trade search, or when "
                 u"you press Refresh on the Trade page. Until then you only see the item's "
                 u"own stats and those on items you've checked."_s);
        QCOMPARE(w.app->statPicker()->rowCount(), equipmentKeys().size());
        QTest::qWait(30);
        QVERIFY(w.network->sent.isEmpty() ||
                !w.network->sent.first().request.url().path().contains(u"/data/"_s));
        w.app->tradeOptions()->refresh();
        QTRY_VERIFY(!isVisible(message));
        QCOMPARE(w.app->statPicker()->rowCount(), equipmentKeys().size() + 4);
    }

    void nothingToSuggest()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QObject *suggest = w.find(u"suggestButton"_s);
        QVERIFY(isVisible(suggest));
        QVERIFY(!suggest->property("enabled").toBool());
        QCOMPARE(suggest->property("description").toString(),
                 u"Your gear has no stats to suggest weights for."_s);
    }

    void useSuggestedWeights()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({{u"Jewel 2"_s, item(jewel)}});
        QObject *suggest = w.find(u"suggestButton"_s);
        QVERIFY(suggest->property("enabled").toBool());
        QCOMPARE(suggest->property("description").toString(), QString());
        QVERIFY(click(suggest));
        QObject *dialog = w.find(u"suggestDialog"_s);
        QVERIFY(isVisible(dialog));

        QObject *fire = w.find(u"suggestion:#% to Fire Resistance"_s);
        QCOMPARE(text(fire), u"#% to Fire Resistance"_s);
        QCOMPARE(fire->property("description").toString(),
                 u"On one of your items, at 8. Your weight for it is 1.5"_s);
        QCOMPARE(text(fire->findChild<QObject *>(u"suggestedWeight"_s)), QLocale().toString(3.8, 'f', 1));
        // as it has a weight already
        QVERIFY(!button(dialog, u"Add the Missing Ones"_s)->property("enabled").toBool());

        QVERIFY(click(button(dialog, u"Use These"_s)));
        QCOMPARE(w.app->character().character().weights, Weights({{u"#% to Fire Resistance"_s, 3.8}}));
        QCOMPARE(text(w.find(u"undoMessage"_s)), u"Your weights are the suggested ones now."_s);
        QTRY_VERIFY(!isVisible(dialog));
    }

    void addSuggestedWeights()
    {
        Window w;
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({{u"Jewel 2"_s, item(luckyJewel)}});
        QVERIFY(click(w.find(u"suggestButton"_s)));
        QObject *dialog = w.find(u"suggestDialog"_s);
        QVERIFY(isVisible(dialog));
        QCOMPARE(
            w.find(u"suggestion:#% increased Rarity of Items found"_s)->property("description").toString(),
            u"On one of your items, at 10"_s);

        QObject *add = button(dialog, u"Add the Missing Ones"_s);
        QVERIFY(add->property("enabled").toBool());
        QVERIFY(click(add));
        const Weights &weights = w.app->character().character().weights;
        QCOMPARE(weights.value(u"#% increased Rarity of Items found"_s), 3.0);
        QCOMPARE(weights.value(u"#% to Fire Resistance"_s), 1.5);
        QCOMPARE(text(w.find(u"undoMessage"_s)), u"Added a suggested weight to yours."_s);
        QTRY_VERIFY(!isVisible(dialog));
        QVERIFY(isVisible(w.find(u"weight:#% increased Rarity of Items found"_s)));
    }

    void keepWeights()
    {
        Window w;
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({{u"Jewel 2"_s, item(luckyJewel)}});
        QVERIFY(click(w.find(u"suggestButton"_s)));
        QObject *dialog = w.find(u"suggestDialog"_s);
        QVERIFY(click(button(dialog, u"Cancel"_s)));
        QCOMPARE(w.app->character().character().weights, defaultWeights());
        QTRY_VERIFY(!isVisible(dialog));
    }

    void cantSaveWeights()
    {
        Window w;
        QVERIFY(w.show(u"weightsAction"_s));
        const QString id = w.play({});
        const QString file = w.dir.filePath(u"data/characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QVERIFY(setValue(w.find(u"weight:# to maximum Life"_s)->findChild<QObject *>(u"weightBox"_s), 2000,
                         "valueModified"));
        QObject *error = w.find(u"weightsError"_s);
        QVERIFY(isVisible(error));
        QVERIFY(text(error).startsWith(u"Couldn't save Wren: "_s));
    }

    void ignoredForNobody()
    {
        Window w;
        QVERIFY(w.show(u"ignoredAction"_s));
        QVERIFY(isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(!isVisible(w.find(u"ignoredError"_s)));
        // the groups can be set all the same
        QVERIFY(isVisible(w.find(u"ignoredHelp"_s)));
        QVERIFY(isVisible(w.find(u"singleStats"_s)));
        QObject *box = w.find(u"group:eleres"_s)->findChild<QObject *>(u"modeBox"_s);
        QVERIFY(activate(box, 1));
        QVERIFY(!isVisible(w.find(u"nobodyText"_s)));
        QCOMPARE(w.app->character().character().name, u"My character"_s);
        QCOMPARE(w.app->character().character().ignored.size(), 1);
    }

    void ignoreGroups()
    {
        Window w;
        QVERIFY(w.show(u"ignoredAction"_s));
        w.play({});
        QVERIFY(!isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(isVisible(w.find(u"ignoredHelp"_s)));
        QVERIFY(isVisible(w.find(u"noSingleText"_s)));
        QObject *eleres = w.find(u"group:eleres"_s);
        QVERIFY(isVisible(eleres));
        QCOMPARE(text(eleres), u"Elemental resistances"_s);
        QCOMPARE(eleres->property("description").toString(),
                 u"#% to Fire Resistance, #% to Cold Resistance, #% to Lightning Resistance"_s);
        QObject *box = eleres->findChild<QObject *>(u"modeBox"_s);
        QCOMPARE(box->property("count").toInt(), 3);
        QCOMPARE(box->property("currentIndex").toInt(), 0);

        QVERIFY(activate(box, 2));
        const IgnoreRule excluded{.preset = u"eleres"_s, .key = {}, .mode = IgnoreRule::Mode::Exclude};
        QCOMPARE(w.app->character().character().ignored, QList<IgnoreRule>({excluded}));
        QCOMPARE(
            w.find(u"group:eleres"_s)->findChild<QObject *>(u"modeBox"_s)->property("currentIndex").toInt(),
            2);
        QVERIFY(activate(w.find(u"group:eleres"_s)->findChild<QObject *>(u"modeBox"_s), 0));
        QVERIFY(w.app->character().character().ignored.isEmpty());

        // and one by itself, excluded elsewhere
        w.app->ignoredStats()->add(u"# to maximum Life"_s);
        w.app->ignoredStats()->setMode(u"# to maximum Life"_s, IgnoredStats::Excluded);
        QTRY_COMPARE(w.find(u"ignored:# to maximum Life"_s)
                         ->findChild<QObject *>(u"modeBox"_s)
                         ->property("currentIndex")
                         .toInt(),
                     1);
    }

    void ignoreAStat()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"ignoredAction"_s));
        w.play({});
        QVERIFY(w.rated());
        QVERIFY(activate(w.find(u"group:eleres"_s)->findChild<QObject *>(u"modeBox"_s), 1));
        QVERIFY(click(w.find(u"ignoreStatButton"_s)));
        QObject *dialog = w.find(u"ignorePicker"_s);
        QVERIFY(isVisible(dialog));
        QCOMPARE(dialog->property("title").toString(), u"Ignore a Stat"_s);

        QObject *search = w.find(u"statSearch"_s);
        QVERIFY(search->setProperty("text", u"fire"_s));
        QVERIFY(w.settled());
        QObject *fire = w.find(u"stat:#% to Fire Resistance"_s);
        QVERIFY(!fire->property("enabled").toBool());
        QCOMPARE(text(fire->findChild<QObject *>(u"statNote"_s)), u"You're ignoring it already"_s);

        QVERIFY(search->setProperty("text", u"armour"_s));
        QVERIFY(w.settled());
        QVERIFY(click(w.find(u"stat:#% increased Armour"_s)));
        QTRY_VERIFY(!isVisible(dialog));
        const QString armour = u"#% increased Armour"_s;
        QCOMPARE(w.app->character().character().ignored.size(), 2);
        QCOMPARE(w.app->character().character().ignored.last().key, armour);
        QVERIFY(!isVisible(w.find(u"noSingleText"_s)));

        QObject *row = w.find(u"ignored:#% increased Armour"_s);
        QVERIFY(isVisible(row));
        QCOMPARE(text(row), armour);
        QObject *box = row->findChild<QObject *>(u"modeBox"_s);
        QCOMPARE(box->property("count").toInt(), 2);
        QCOMPARE(box->property("currentIndex").toInt(), 0);
        QVERIFY(activate(box, 1));
        QCOMPARE(w.app->character().character().ignored.last().mode, IgnoreRule::Mode::Exclude);
        QCOMPARE(w.find(u"ignored:#% increased Armour"_s)
                     ->findChild<QObject *>(u"modeBox"_s)
                     ->property("currentIndex")
                     .toInt(),
                 1);

        QVERIFY(click(w.find(u"ignored:#% increased Armour"_s)->findChild<QObject *>(u"removeButton"_s)));
        QCOMPARE(w.app->character().character().ignored.size(), 1);
        QTRY_VERIFY(!::search(w.page(),
                              [](QObject *o) { return o->objectName() == u"ignored:#% increased Armour"; }));
        QVERIFY(isVisible(w.find(u"noSingleText"_s)));
    }

    void cantSaveIgnored()
    {
        Window w;
        QVERIFY(w.show(u"ignoredAction"_s));
        const QString id = w.play({});
        const QString file = w.dir.filePath(u"data/characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QVERIFY(activate(w.find(u"group:life"_s)->findChild<QObject *>(u"modeBox"_s), 1));
        QObject *error = w.find(u"ignoredError"_s);
        QVERIFY(isVisible(error));
        QVERIFY(text(error).startsWith(u"Couldn't save Wren: "_s));
    }

    void nothingChecked()
    {
        Window w;
        QVERIFY(w.show(u"checkedAction"_s));
        QObject *none = w.find(u"noItemsText"_s);
        QVERIFY(isVisible(none));
        QCOMPARE(text(none), u"You haven't checked any items yet."_s);
        QCOMPARE(none->property("description").toString(),
                 u"Hover over an item in Path of Exile 2 and press F2."_s);
        QCOMPARE(w.find(u"checkedHeader"_s)->property("title").toString(), u"Recently checked"_s);
        QVERIFY(!isVisible(w.find(u"forgetSeparator"_s)));
        QVERIFY(!isVisible(w.find(u"forgetButton"_s)));
        QVERIFY(!isVisible(w.find(u"checkedHelp"_s)));
        QVERIFY(!search(w.page(), [](QObject *o) { return o->objectName().startsWith(u"checked:"); }));

        // with the key it has now
        QVERIFY(w.app->overlayOptions()->setCheckKey(QKeySequence(Qt::Key_F6)));
        QCOMPARE(none->property("description").toString(),
                 u"Hover over an item in Path of Exile 2 and press F6."_s);
    }

    void nothingCheckedWithoutHotkeys()
    {
        Window w(false);
        QVERIFY(w.show(u"checkedAction"_s));
        QCOMPARE(w.find(u"noItemsText"_s)->property("description").toString(),
                 u"Hover over an item in Path of Exile 2 and press your check hotkey. You can set it on "
                 "the General page."_s);
    }
};

QTEST_MAIN(TestMainWindowGear)
#include "tst_mainwindow_gear.moc"
