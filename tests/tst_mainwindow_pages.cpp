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

class TestMainWindowPages : public QObject
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

    void opens()
    {
        Window w;
        QVERIFY(w.window);
        QCOMPARE(w.window->title(), u"Path of Upgrading"_s);
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
    }

    void showsItself()
    {
        Window w;
        QVERIFY(w.window->isVisible());
    }

    void startsHidden()
    {
        Window w(true, true);
        QVERIFY(w.window);
        QVERIFY(!w.window->isVisible());
        // and is there when the tray asks
        w.window->show();
        QVERIFY(w.window->isVisible());
    }

    void noCharacters()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(isVisible(w.find(u"noCharactersText"_s)));
        QVERIFY(!isVisible(w.find(u"charactersError"_s)));
        QVERIFY(!isVisible(w.find(u"problemsText"_s)));
        QVERIFY(!isVisible(w.find(u"importMessage"_s)));
        // without Path of Building
        QVERIFY(!isVisible(w.find(u"savedBuilds"_s)));
    }

    void addCharacter()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.shown());
        QObject *name = w.find(u"nameField"_s);
        QObject *add = w.find(u"addButton"_s);
        QVERIFY(!add->property("enabled").toBool());
        QVERIFY(name->setProperty("text", u" Wren "_s));
        QVERIFY(add->property("enabled").toBool());
        QVERIFY(click(add));
        const CharacterList *list = w.app->characterList();
        QCOMPARE(list->rowCount(), 1);
        QCOMPARE(w.settings().character, list->current());
        QCOMPARE(text(name), QString());

        QObject *wren = w.find(u"character:Wren"_s);
        QVERIFY(wren->property("checked").toBool());
        QCOMPARE(wren->property("description").toString(), u"No build imported"_s);
        QVERIFY(!isVisible(w.find(u"noCharactersText"_s)));

        // with Enter
        QVERIFY(name->setProperty("text", u"Ash"_s));
        QVERIFY(QMetaObject::invokeMethod(name, "accepted"));
        QCOMPARE(list->rowCount(), 2);
        QVERIFY(w.find(u"character:Ash"_s)->property("checked").toBool());
        QVERIFY(!wren->property("checked").toBool());
    }

    void buttonsAfterLeaving()
    {
        // which the pages had while they were away
        Window w;
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.show(u"gearAction"_s));
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(isVisible(w.find(u"character:Wren"_s)->findChild<QObject *>(u"renameButton"_s)));
        QVERIFY(isVisible(w.find(u"character:Wren"_s)->findChild<QObject *>(u"removeButton"_s)));
        QVERIFY(w.show(u"gearAction"_s));
        QVERIFY(isVisible(w.find(u"slot:Helmet"_s)->findChild<QObject *>(u"takeOffButton"_s)));
    }

    void addWithoutName()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        QObject *name = w.find(u"nameField"_s);
        QVERIFY(name->setProperty("text", u"  "_s));
        QVERIFY(QMetaObject::invokeMethod(name, "accepted"));
        QCOMPARE(w.app->characterList()->rowCount(), 0);
        QObject *error = w.find(u"charactersError"_s);
        QVERIFY(isVisible(error));
        QCOMPARE(text(error), u"Give the character a name."_s);
    }

    void characterPickerOnTheGearPage()
    {
        Window w;
        QObject *box = w.find(u"characterBox"_s);
        QVERIFY(box);
        // there's nobody to pick yet
        QVERIFY(!isVisible(box));

        CharacterList *list = w.app->characterList();
        list->add(u"Wren"_s);
        const QString ash = list->add(u"Ash"_s);
        QTRY_VERIFY(isVisible(box));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QCOMPARE(box->property("count").toInt(), 2);
        QCOMPARE(box->property("currentText").toString(), u"Ash"_s);

        // picking plays the one it names
        QVERIFY(activate(box, 1));
        QCOMPARE(list->current(), list->data(list->index(1), CharacterList::IdRole).toString());
        QCOMPARE(box->property("currentText").toString(), u"Wren"_s);
        QCOMPARE(w.settings().character, list->current());
        QVERIFY(list->current() != ash);

        // and the characters page says the same
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.find(u"character:Wren"_s)->property("checked").toBool());
        QVERIFY(!w.find(u"character:Ash"_s)->property("checked").toBool());
    }

    void pickCharacter()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        CharacterList *list = w.app->characterList();
        const QString wren = list->add(u"Wren"_s);
        list->add(u"Ash"_s);
        QVERIFY(click(w.find(u"character:Wren"_s)));
        QCOMPARE(list->current(), wren);
        QCOMPARE(w.settings().character, wren);
        QVERIFY(w.find(u"character:Wren"_s)->property("checked").toBool());
        QVERIFY(!w.find(u"character:Ash"_s)->property("checked").toBool());

        // and again, which leaves it picked
        QVERIFY(click(w.find(u"character:Wren"_s)));
        QVERIFY(w.find(u"character:Wren"_s)->property("checked").toBool());

        // or by its radio button
        QObject *ash = w.find(u"character:Ash"_s)->findChild<QObject *>(u"pickButton"_s);
        QVERIFY(!ash->property("checked").toBool());
        QVERIFY(click(ash));
        QVERIFY(ash->property("checked").toBool());
        QVERIFY(!w.find(u"character:Wren"_s)
                     ->findChild<QObject *>(u"pickButton"_s)
                     ->property("checked")
                     .toBool());
    }

    void viewGear()
    {
        Window w;
        CharacterList *list = w.app->characterList();
        const QString wren = list->add(u"Wren"_s);
        const QString ash = list->add(u"Ash"_s);
        QVERIFY(w.show(u"charactersAction"_s));
        QCOMPARE(list->current(), ash);

        // plays the one it is on, and shows what it wears
        QVERIFY(click(w.find(u"character:Wren"_s)->findChild<QObject *>(u"gearButton"_s)));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QCOMPARE(list->current(), wren);
        QCOMPARE(w.settings().character, wren);
        QVERIFY(w.window->findChild<QObject *>(u"gearAction"_s)->property("checked").toBool());
        QVERIFY(!w.window->findChild<QObject *>(u"charactersAction"_s)->property("checked").toBool());

        // the others are as they were
        QCOMPARE(list->rowCount(), 2);
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.find(u"character:Wren"_s)->property("checked").toBool());
        QVERIFY(!w.find(u"character:Ash"_s)->property("checked").toBool());

        // the one that's played already just opens it
        QVERIFY(click(w.find(u"character:Wren"_s)->findChild<QObject *>(u"gearButton"_s)));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QCOMPARE(list->current(), wren);
    }

    void gearButtonName()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        w.app->characterList()->add(u"Wren"_s);
        QObject *button = w.find(u"character:Wren"_s)->findChild<QObject *>(u"gearButton"_s);
        QCOMPARE(text(button), u"View Gear"_s);
        QCOMPARE(spoken(button), u"View Gear of Wren"_s);
    }

    void characterButtonNames()
    {
        // which are icons that all look alike, so each says whose it is
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        w.app->characterList()->add(u"Wren"_s);
        QObject *row = w.find(u"character:Wren"_s);
        QCOMPARE(spoken(row->findChild<QObject *>(u"renameButton"_s)), u"Rename Wren"_s);
        QCOMPARE(spoken(row->findChild<QObject *>(u"removeButton"_s)), u"Remove Wren"_s);
    }

    void renameCharacter()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        CharacterList *list = w.app->characterList();
        const QString wren = list->add(u"Wren"_s);
        QVERIFY(click(w.find(u"character:Wren"_s)->findChild<QObject *>(u"renameButton"_s)));
        QObject *dialog = w.find(u"renameDialog"_s);
        QVERIFY(isVisible(dialog));
        QObject *field = w.find(u"renameField"_s);
        QCOMPARE(text(field), u"Wren"_s);

        QObject *rename = button(dialog, u"Rename"_s);
        QVERIFY(rename);
        QVERIFY(field->setProperty("text", u" "_s));
        QVERIFY(!rename->property("enabled").toBool());
        // which Enter doesn't do either
        QVERIFY(QMetaObject::invokeMethod(field, "accepted"));
        QVERIFY(isVisible(dialog));
        QVERIFY(field->setProperty("text", u"Ashen Wren"_s));
        QVERIFY(click(rename));
        QCOMPARE(list->find(wren)->name, u"Ashen Wren"_s);
        QVERIFY(w.find(u"character:Ashen Wren"_s));
        QTRY_VERIFY(!isVisible(dialog));

        // with Enter
        QVERIFY(click(w.find(u"character:Ashen Wren"_s)->findChild<QObject *>(u"renameButton"_s)));
        QVERIFY(field->setProperty("text", u"Wren"_s));
        QVERIFY(QMetaObject::invokeMethod(field, "accepted"));
        QCOMPARE(list->find(wren)->name, u"Wren"_s);
        QTRY_VERIFY(!isVisible(dialog));
    }

    void removeCharacter()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        CharacterList *list = w.app->characterList();
        list->add(u"Wren"_s);
        QVERIFY(click(w.find(u"character:Wren"_s)->findChild<QObject *>(u"removeButton"_s)));
        QObject *dialog = w.find(u"removeDialog"_s);
        QVERIFY(isVisible(dialog));
        QCOMPARE(dialog->property("title").toString(), u"Remove Wren?"_s);
        QVERIFY(click(button(dialog, u"Remove"_s)));
        QCOMPARE(list->rowCount(), 0);
        QVERIFY(isVisible(w.find(u"noCharactersText"_s)));
        QTRY_VERIFY(!isVisible(dialog));
    }

    void keepCharacter()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        CharacterList *list = w.app->characterList();
        list->add(u"Wren"_s);
        QVERIFY(click(w.find(u"character:Wren"_s)->findChild<QObject *>(u"removeButton"_s)));
        QObject *dialog = w.find(u"removeDialog"_s);
        QVERIFY(click(button(dialog, u"Cancel"_s)));
        QCOMPARE(list->rowCount(), 1);
        QTRY_VERIFY(!isVisible(dialog));
    }

    void importBuild()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        QObject *code = w.find(u"codeField"_s);
        QVERIFY(code->setProperty("text", ranger));
        QVERIFY(click(w.find(u"importButton"_s)));
        QObject *message = w.find(u"importMessage"_s);
        QVERIFY(isVisible(message));
        QCOMPARE(text(message), u"Imported the level 40 Ranger build for Ranger. The build has no items."_s);
        QCOMPARE(text(code), QString());

        // on a new one, as there was none to play
        QObject *row = w.find(u"character:Ranger"_s);
        QVERIFY(row->property("checked").toBool());
        QCOMPARE(row->property("description").toString(), u"Level 40 Ranger"_s);
    }

    void importFails()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        QObject *code = w.find(u"codeField"_s);
        QVERIFY(code->setProperty("text", u"nonsense"_s));
        QVERIFY(QMetaObject::invokeMethod(code, "accepted"));
        QCOMPARE(text(w.find(u"importMessage"_s)),
                 u"That doesn't look like a Path of Building code or a link to one."_s);
        // to put right
        QCOMPARE(text(code), u"nonsense"_s);
        QCOMPARE(w.app->characterList()->rowCount(), 0);
    }

    void downloading()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        // with the sites answered first
        QVERIFY(w.rated());
        QObject *code = w.find(u"codeField"_s);
        QObject *import = w.find(u"importButton"_s);
        QVERIFY(code->setProperty("text", u"https://pobb.in/abc"_s));
        QVERIFY(click(import));
        QCOMPARE(text(import), u"Downloading the Build"_s);
        QVERIFY(!import->property("enabled").toBool());
        QVERIFY(!w.find(u"fileButton"_s)->property("enabled").toBool());

        QTRY_VERIFY(import->property("enabled").toBool());
        QCOMPARE(text(import), u"Import"_s);
        QVERIFY(w.app->buildImporter()->hasFailed());
        QCOMPARE(text(code), u"https://pobb.in/abc"_s);
    }

    void savedBuilds()
    {
        Window w;
        QVERIFY(w.show(u"charactersAction"_s));
        const QString folder = w.pob.folder.path();
        QVERIFY(makePob(folder));
        w.app->pobOptions()->setLocation(folder);
        QVERIFY(isVisible(w.find(u"savedBuilds"_s)));
        const QString builds = w.app->buildImporter()->buildsFolder();
        QCOMPARE(text(w.find(u"noSavedBuildsText"_s)), u"There are no builds in %1 yet."_s.arg(builds));

        QVERIFY(writeFile(builds + u"/Leveling.xml"_s, ranger.toUtf8()));
        QVERIFY(click(w.find(u"refreshBuildsButton"_s)));
        QVERIFY(!isVisible(w.find(u"noSavedBuildsText"_s)));
        QObject *saved = w.find(u"saved:Leveling"_s);
        const QDateTime modified = QFileInfo(builds + u"/Leveling.xml"_s).lastModified();
        QCOMPARE(saved->property("description").toString(),
                 u"Saved %1"_s.arg(QLocale().toString(modified, QLocale::ShortFormat)));
        QVERIFY(click(saved));
        QCOMPARE(text(w.find(u"importMessage"_s)),
                 u"Imported the level 40 Ranger build for Leveling. The build has no items."_s);
    }

    // as for one saved while you were on another page
    void savedSince()
    {
        Window w;
        const QString folder = w.pob.folder.path();
        QVERIFY(makePob(folder));
        w.app->pobOptions()->setLocation(folder);
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(writeFile(w.app->buildImporter()->buildsFolder() + u"/Leveling.xml"_s, ranger.toUtf8()));
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.find(u"saved:Leveling"_s));
    }

    void pobNotFound()
    {
        Window w;
        QVERIFY(w.show(u"pobAction"_s));
        QCOMPARE(text(w.find(u"foundText"_s)), u"Path of Building isn't anywhere the app looks."_s);
        QVERIFY(w.find(u"stepsText"_s)->property("description").toString().startsWith(u"1. "_s));
        QVERIFY(!isVisible(w.find(u"installedAtText"_s)));
        // nothing is opened or copied until you click for it
        QVERIFY(w.opener->opened.isEmpty());
        QVERIFY(w.opener->copied.isEmpty());
    }

    // for someone who has never heard of a Flatpak
    void pobGettingIt()
    {
        Window w;
        QVERIFY(w.show(u"pobAction"_s));
        QCOMPARE(w.find(u"copyCommandButton"_s)->property("description").toString(),
                 u"flatpak install flathub community.pathofbuilding.PathOfBuilding"_s);
        QVERIFY(!isVisible(w.find(u"installerButton"_s)));

        QVERIFY(click(w.find(u"discoverButton"_s)));
        QCOMPARE(w.opener->opened,
                 QList<QUrl>{QUrl(u"appstream://community.pathofbuilding.PathOfBuilding"_s)});
        QVERIFY(click(w.find(u"flathubButton"_s)));
        QCOMPARE(w.opener->opened.last(),
                 QUrl(u"https://flathub.org/apps/community.pathofbuilding.PathOfBuilding"_s));
        QVERIFY(w.opener->copied.isEmpty());
        QVERIFY(click(w.find(u"copyCommandButton"_s)));
        QCOMPARE(w.opener->copied,
                 QStringList{u"flatpak install flathub community.pathofbuilding.PathOfBuilding"_s});
    }

    void pobFolder()
    {
        Window w;
        QVERIFY(w.show(u"pobAction"_s));
        const QString folder = w.pob.folder.path();
        QVERIFY(makePob(folder));
        QObject *field = w.find(u"folderField"_s);
        QVERIFY(field->setProperty("text", folder));
        QVERIFY(QMetaObject::invokeMethod(field, "editingFinished"));
        QCOMPARE(w.settings().pob.location, folder);
        QCOMPARE(text(w.find(u"foundText"_s)),
                 u"You're using the Path of Building in the folder you chose."_s);
        QCOMPARE(w.find(u"installedAtText"_s)->property("description").toString(),
                 QDir::toNativeSeparators(folder));
        QVERIFY(isVisible(w.find(u"installedAtText"_s)));
        QVERIFY(isVisible(w.find(u"buildsAtText"_s)));
        QVERIFY(!isVisible(w.find(u"stepsText"_s)));
        QVERIFY(!isVisible(w.find(u"discoverButton"_s)));
    }

    void pobFolderNoGood()
    {
        Window w;
        QVERIFY(w.show(u"pobAction"_s));
        QVERIFY(!isVisible(w.find(u"usualPlacesButton"_s)));
        // there's no Path of Building in it
        w.app->pobOptions()->setLocation(w.pob.folder.path());
        QCOMPARE(text(w.find(u"foundText"_s)), u"There's no Path of Building in the folder you chose."_s);
        QVERIFY(text(w.find(u"folderNote"_s)).contains(u"HeadlessWrapper.lua"_s));
        QVERIFY(isVisible(w.find(u"usualPlacesButton"_s)));
        QVERIFY(click(w.find(u"usualPlacesButton"_s)));
        QCOMPARE(w.settings().pob.location, QString());
        QCOMPARE(text(w.find(u"folderField"_s)), QString());
        QCOMPARE(text(w.find(u"foundText"_s)), u"Path of Building isn't anywhere the app looks."_s);
        QVERIFY(!isVisible(w.find(u"usualPlacesButton"_s)));
    }

    void pobWhereItLooks()
    {
        Window w;
        QVERIFY(w.show(u"pobAction"_s));
        QVERIFY(!isVisible(w.find(u"placesText"_s)));
        QCOMPARE(text(w.find(u"placesButton"_s)), u"Show Where It Looks"_s);
        QVERIFY(click(w.find(u"placesButton"_s)));
        QVERIFY(isVisible(w.find(u"placesText"_s)));
        QCOMPARE(text(w.find(u"placesButton"_s)), u"Hide Where It Looks"_s);
        QVERIFY(text(w.find(u"placesText"_s)).contains(u"Not from the Flatpak: "_s));
        QVERIFY(click(w.find(u"placesButton"_s)));
        QVERIFY(!isVisible(w.find(u"placesText"_s)));
    }

    void lookAgain()
    {
        Window w;
        QVERIFY(w.show(u"pobAction"_s));
        const QString folder = w.pob.folder.path();
        w.app->pobOptions()->setLocation(folder);
        QCOMPARE(text(w.find(u"foundText"_s)), u"There's no Path of Building in the folder you chose."_s);

        // once it's there
        QVERIFY(makePob(folder));
        QVERIFY(QMetaObject::invokeMethod(w.find(u"updateButton"_s), "clicked"));
        QCOMPARE(text(w.find(u"foundText"_s)),
                 u"You're using the Path of Building in the folder you chose."_s);
    }

    void pobSettings()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"pobAction"_s));
        const QStringList weighing = {u"damageSlider"_s,     u"resistsSwitch"_s,     u"chaosSwitch"_s,
                                      u"keepDamageSwitch"_s, u"keepDefenceSwitch"_s, u"spiritSwitch"_s,
                                      u"attributesSwitch"_s};
        for (const QString &name : weighing)
            QVERIFY2(w.find(name)->property("enabled").toBool(), qPrintable(name));
        QVERIFY(toggle(w.find(u"chaosSwitch"_s)));
        QVERIFY(toggle(w.find(u"keepDamageSwitch"_s)));
        QVERIFY(toggle(w.find(u"keepDefenceSwitch"_s)));
        QVERIFY(toggle(w.find(u"spiritSwitch"_s)));
        QVERIFY(toggle(w.find(u"attributesSwitch"_s)));
        const PobPriorities &priorities = w.settings().pob.priorities;
        QCOMPARE(priorities.chaos, true);
        QCOMPARE(priorities.keepDamage, false);
        QCOMPARE(priorities.keepDefence, false);
        QCOMPARE(priorities.spirit, false);
        QCOMPARE(priorities.attributes, false);

        QVERIFY(toggle(w.find(u"enabledSwitch"_s)));
        QCOMPARE(w.settings().pob.enabled, false);
        // which all of them need
        for (const QString &name : weighing)
            QVERIFY2(!w.find(name)->property("enabled").toBool(), qPrintable(name));
        QVERIFY(w.find(u"folderField"_s)->property("enabled").toBool());
        QVERIFY(toggle(w.find(u"enabledSwitch"_s)));
        for (const QString &name : weighing)
            QVERIFY2(w.find(name)->property("enabled").toBool(), qPrintable(name));
        QVERIFY(toggle(w.find(u"enabledSwitch"_s)));

        QVERIFY(toggle(w.find(u"resistsSwitch"_s)));
        QCOMPARE(w.settings().pob.priorities.resistsFirst, false);
        QVERIFY(setValue(w.find(u"damageSlider"_s), 0.3, "moved"));
        QCOMPARE(w.settings().pob.damageWeight, 0.3);
    }

    // as you left them last time
    void showsPobSettings()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"pobAction"_s));
        Settings settings = w.settings();
        settings.pob.enabled = false;
        settings.pob.location = w.pob.folder.path();
        settings.pob.damageWeight = 0.8;
        settings.pob.priorities.resistsFirst = false;
        QVERIFY(w.app->settings().set(settings));
        QCOMPARE(w.find(u"enabledSwitch"_s)->property("checked").toBool(), false);
        QCOMPARE(text(w.find(u"folderField"_s)), w.pob.folder.path());
        QCOMPARE(w.find(u"damageSlider"_s)->property("value").toDouble(), 0.8);
        QCOMPARE(w.find(u"resistsSwitch"_s)->property("checked").toBool(), false);
        // which only counts toward the caps
        QCOMPARE(w.find(u"chaosSwitch"_s)->property("enabled").toBool(), false);
    }

    // A click on its switch or on the rest of the row flips it, and it still
    // follows the setting when that changes somewhere else.
    void switchRows()
    {
        Window w;
        QVERIFY(w.shown());
        QVERIFY(w.show(u"pobAction"_s));
        QVERIFY(w.settled());
        auto *row = qobject_cast<QQuickItem *>(w.find(u"enabledSwitch"_s));
        QVERIFY(row);
        auto *flip = row->findChild<QQuickItem *>(u"switch"_s);
        QVERIFY(flip);
        QCOMPARE(row->property("checked").toBool(), true);

        QTest::mouseClick(w.window.get(), Qt::LeftButton, {},
                          flip->mapToScene(QPointF(flip->width() / 2, flip->height() / 2)).toPoint());
        QCOMPARE(w.settings().pob.enabled, false);
        QCOMPARE(row->property("checked").toBool(), false);
        QCOMPARE(flip->property("checked").toBool(), false);

        QTest::mouseClick(w.window.get(), Qt::LeftButton, {},
                          row->mapToScene(QPointF(20, row->height() / 2)).toPoint());
        QCOMPARE(w.settings().pob.enabled, true);
        QCOMPARE(row->property("checked").toBool(), true);
        QCOMPARE(flip->property("checked").toBool(), true);

        Settings settings = w.settings();
        settings.pob.enabled = false;
        QVERIFY(w.app->settings().set(settings));
        QCOMPARE(row->property("checked").toBool(), false);
        QCOMPARE(flip->property("checked").toBool(), false);
    }

    void pages()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Trade"_s);
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"General"_s);
        QVERIFY(w.show(u"pobAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Build Review"_s);
        QVERIFY(w.show(u"gearAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QVERIFY(w.show(u"gearScoreAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Gear Score"_s);
        QVERIFY(w.show(u"weightsAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Stat Weights"_s);
        QVERIFY(w.show(u"ignoredAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Ignored Stats"_s);
        QVERIFY(w.show(u"checkedAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Item History"_s);
        QVERIFY(w.show(u"charactersAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Characters"_s);
    }

    void gearForNobody()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        QVERIFY(isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(!isVisible(w.find(u"overallScore"_s)));
        QVERIFY(!isVisible(w.find(u"gearError"_s)));
        QVERIFY(!isVisible(w.find(u"extras"_s)));
        // the slots are there to fill, and the rest is as for anyone
        QVERIFY(isVisible(w.find(u"gearHelp"_s)));
        QVERIFY(isVisible(w.find(u"resistances"_s)));
        QObject *helmet = w.find(u"slot:Helmet"_s);
        QVERIFY(isVisible(helmet));
        QCOMPARE(helmet->property("description").toString(), u"Nothing saved yet"_s);

        // and wearing something makes a character
        QVERIFY(w.app->character().wear(u"Helmet"_s, item(itemtexts::helmetOld)));
        QVERIFY(!isVisible(w.find(u"nobodyText"_s)));
        QCOMPARE(w.find(u"slot:Helmet"_s)->property("description").toString(),
                 u"Grim Crown, Visored Helm, item level 50"_s);
        QVERIFY(isVisible(w.find(u"overallScore"_s)));
    }

    void gear()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}, {u"Ring 1"_s, item(itemtexts::advancedRing)}},
               {u"Ring 1"_s});
        QVERIFY(!isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(isVisible(w.find(u"gearHelp"_s)));
        QVERIFY(!isVisible(w.find(u"extras"_s)));

        QObject *helmet = w.find(u"slot:Helmet"_s);
        QVERIFY(isVisible(helmet));
        QCOMPARE(text(helmet), u"Helmet"_s);
        QCOMPARE(helmet->property("description").toString(), u"Grim Crown, Visored Helm, item level 50"_s);
        // the slot, then the item and its base, each on its own line
        QCOMPARE(text(helmet->findChild<QObject *>(u"slotName"_s)), u"Helmet"_s);
        QCOMPARE(text(helmet->findChild<QObject *>(u"itemName"_s)), u"Grim Crown"_s);
        QObject *base = helmet->findChild<QObject *>(u"itemBase"_s);
        QVERIFY(isVisible(base));
        QCOMPARE(text(base), u"Visored Helm, item level 50"_s);
        QObject *score = search(helmet, [](QObject *o) { return o->objectName() == u"gearScore"; });
        QVERIFY(isVisible(score));
        const QString number = w.app->gearList()->index(2).data(GearList::ScoreRole).toString();
        const QString grade = w.app->gearList()->index(2).data(GearList::GradeRole).toString();
        QVERIFY(!number.isEmpty());
        QCOMPARE(text(score), u"Gear score %1 %2"_s.arg(number, grade));
        QVERIFY(isVisible(helmet->findChild<QObject *>(u"takeOffButton"_s)));

        QCOMPARE(w.find(u"slot:Ring 1"_s)->property("description").toString(),
                 u"Storm Loop, Sapphire Ring, item level 75, from Path of Building"_s);

        // what would raise its score, and what an empty slot should have
        // each way it falls short on a line of its own, with the stats beside the label
        const auto notes = w.app->gearList()->index(2).data(GearList::NotesRole).value<QList<GearNote>>();
        QVERIFY(!notes.isEmpty());
        QCOMPARE(notes.first().label, u"Missing stats"_s);
        const QList<QObject *> labels = findAll(helmet, u"noteLabel"_s);
        const QList<QObject *> texts = findAll(helmet, u"noteText"_s);
        QCOMPARE(labels.size(), notes.size());
        QCOMPARE(texts.size(), notes.size());
        for (qsizetype i = 0; i < notes.size(); ++i) {
            QVERIFY(isVisible(labels.at(i)));
            QCOMPARE(text(labels.at(i)), notes.at(i).label);
            // which says what it means when hovered
            QVERIFY(!notes.at(i).help.isEmpty());
            QCOMPARE(labels.at(i)->property("help").toString(), notes.at(i).help);
            QCOMPARE(text(texts.at(i)), notes.at(i).text);
            // a chip to each stat
            QStringList chips;
            for (QObject *chip : findAll(texts.at(i), u"statChip"_s)) {
                QVERIFY(isVisible(chip));
                chips << text(chip);
            }
            QStringList stats = notes.at(i).stats;
            chips.sort();
            stats.sort();
            QCOMPARE(chips, stats);
        }

        QObject *gloves = w.find(u"slot:Gloves"_s);
        QCOMPARE(gloves->property("description").toString(), u"Nothing saved yet"_s);
        QCOMPARE(text(gloves->findChild<QObject *>(u"itemName"_s)), u"Nothing saved yet"_s);
        QVERIFY(!isVisible(gloves->findChild<QObject *>(u"itemBase"_s)));
        QObject *wantsLabel = findAll(gloves, u"noteLabel"_s).value(0);
        QVERIFY(isVisible(wantsLabel));
        QCOMPARE(text(wantsLabel), u"Your build wants"_s);
        QVERIFY(!wantsLabel->property("help").toString().isEmpty());
        QVERIFY(!text(findAll(gloves, u"noteText"_s).value(0)).isEmpty());
        QVERIFY(!isVisible(search(gloves, [](QObject *o) { return o->objectName() == u"gearScore"; })));
        QVERIFY(!isVisible(gloves->findChild<QObject *>(u"takeOffButton"_s)));

        QObject *overall = w.find(u"overallScore"_s);
        QVERIFY(isVisible(overall));
        const GearList *list = w.app->gearList();
        QVERIFY(!list->overall().isEmpty());
        QCOMPARE(text(overall), u"Overall gear score %1 %2"_s.arg(list->overall(), list->overallGrade()));
    }

    void recommendedUpgrades()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        QObject *card = w.find(u"recommendedUpgrades"_s);

        // empty slots first, each saying what to look for, and nothing to search
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}, {u"Ring 1"_s, item(itemtexts::advancedRing)}});
        const RecommendedUpgrades *upgrades = w.app->recommendedUpgrades();
        QCOMPARE(upgrades->rowCount(), 3);
        QVERIFY(isVisible(card));
        for (int i = 0; i < upgrades->rowCount(); ++i) {
            const QString slot = upgrades->index(i).data(RecommendedUpgrades::SlotRole).toString();
            QObject *row = w.find(u"recommendation:"_s + slot);
            QVERIFY(isVisible(row));
            QCOMPARE(text(row->findChild<QObject *>(u"slotName"_s)), slotLabel(slot));
            QCOMPARE(text(row->findChild<QObject *>(u"itemName"_s)), u"Nothing saved yet"_s);
            const QString lookFor = upgrades->index(i).data(RecommendedUpgrades::LookForRole).toString();
            QVERIFY(lookFor.contains(u" or more"_s));
            QCOMPARE(text(row->findChild<QObject *>(u"lookFor"_s)), lookFor);
            QCOMPARE(row->findChild<QObject *>(u"lookForLabel"_s)->property("help").toString(),
                     upgrades->lookForHelp());
            QVERIFY(!isVisible(row->findChild<QObject *>(u"findButton"_s)));
            QVERIFY(!isVisible(row->findChild<QObject *>(u"tradeStatus"_s)));
        }

        // with every main slot worn, the lowest gear scores
        w.play({{u"Weapon"_s, item(itemtexts::helmetOld)},
                {u"Offhand"_s, item(itemtexts::helmetOld)},
                {u"Helmet"_s, item(itemtexts::helmetOld)},
                {u"Body Armour"_s, item(itemtexts::helmetOld)},
                {u"Gloves"_s, item(itemtexts::helmetOld)},
                {u"Boots"_s, item(itemtexts::helmetOld)},
                {u"Belt"_s, item(itemtexts::helmetOld)},
                {u"Amulet"_s, item(itemtexts::helmetOld)},
                {u"Ring 1"_s, item(itemtexts::advancedRing)},
                {u"Ring 2"_s, item(itemtexts::advancedRing)}});
        QCOMPARE(upgrades->rowCount(), 3);
        const QString slot = upgrades->index(0).data(RecommendedUpgrades::SlotRole).toString();
        QObject *row = w.find(u"recommendation:"_s + slot);
        QCOMPARE(text(row->findChild<QObject *>(u"itemName"_s)),
                 upgrades->index(0).data(RecommendedUpgrades::NameRole).toString());
        QVERIFY(isVisible(row->findChild<QObject *>(u"gearScore"_s)));

        // trade searches are on, so it offers one, which says how it's going
        QVERIFY(upgrades->isTradeAvailable());
        QObject *find = row->findChild<QObject *>(u"findButton"_s);
        QVERIFY(isVisible(find));
        w.network->answers = {{.never = true}};
        QVERIFY(QMetaObject::invokeMethod(find, "clicked"));
        QVERIFY(isVisible(row->findChild<QObject *>(u"tradeStatus"_s)));
        QCOMPARE(text(row->findChild<QObject *>(u"tradeStatusText"_s)), u"Searching the trade site"_s);
        QVERIFY(!find->property("enabled").toBool());
        QVERIFY(!isVisible(row->findChild<QObject *>(u"showResultsButton"_s)));

        // and with trade off, none
        Settings settings = w.settings();
        settings.trade.enabled = false;
        QVERIFY(w.app->settings().set(settings));
        QVERIFY(!isVisible(w.find(u"recommendation:"_s + slot)->findChild<QObject *>(u"findButton"_s)));
    }

    void gearHeading()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        QObject *heading = search(w.page(), [](QObject *o) {
            return o->inherits("QQuickItem") && o->property("title").toString().startsWith(u"What "_s);
        });
        QCOMPARE(heading->property("title").toString(), u"What you have equipped"_s);
        w.play({});
        QCOMPARE(heading->property("title").toString(), u"What Wren has equipped"_s);
    }

    void gearExtras()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        w.play({{u"Jewel 2"_s, item(jewel)}});
        QVERIFY(isVisible(w.find(u"extras"_s)));
        QObject *row = w.in(u"extras"_s, u"slot:Jewel 2"_s);
        QVERIFY(isVisible(row));
        QCOMPARE(row->property("description").toString(), u"Pale Eye, Ruby"_s);
        // as the card was hidden when it had nothing
        QVERIFY(isVisible(row->findChild<QObject *>(u"takeOffButton"_s)));
        // and only there
        QVERIFY(!isVisible(w.find(u"slot:Jewel 2"_s)));
        QVERIFY(!isVisible(w.in(u"extras"_s, u"slot:Helmet"_s)));
    }

    void gearListWhenNarrow()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        QVERIFY(w.shown());
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.settled());
        QVERIFY(!isVisible(w.find(u"equipment"_s)));
        QVERIFY(isVisible(w.find(u"slot:Helmet"_s)));
        QVERIFY(isVisible(w.find(u"slot:Gloves"_s)));

        // and the list goes when it's wide
        w.window->resize(1500, 900);
        QTRY_VERIFY(w.settled() && isVisible(w.find(u"equipment"_s)));
        QVERIFY(!isVisible(w.find(u"slot:Helmet"_s)));
        QVERIFY(!isVisible(w.find(u"slot:Gloves"_s)));
        w.window->resize(1000, 700);
        QTRY_VERIFY(w.settled() && !isVisible(w.find(u"equipment"_s)));
        QVERIFY(isVisible(w.find(u"slot:Helmet"_s)));
    }

    // A page of the window at a width, once it has been laid out for it.
    static bool pageAt(Window &w, const QString &action, int width)
    {
        if (!w.show(action) || !w.shown())
            return false;
        w.window->resize(width, 900);
        return QTest::qWaitFor(
            [&] { return w.settled() && w.page()->property("wide").toBool() == (width >= 1500); }, 2000);
    }

    // every row of a table with this start of its name
    static QList<QObject *> rowsStartingWith(Window &w, const QString &prefix)
    {
        QList<QObject *> rows;
        search(w.page(), [&](QObject *o) {
            if (o->objectName().startsWith(prefix))
                rows << o;
            return false;
        });
        return rows;
    }

    // That the rows of a table line up with its header on a wide window, and
    // go back to a note under the name on a narrow one.
    static void checkTable(Window &w, const QString &rowPrefix, const QString &header, bool wide)
    {
        const QList<QObject *> rows = rowsStartingWith(w, rowPrefix);
        QVERIFY(!rows.isEmpty());
        QObject *head = w.find(header);
        QCOMPARE(isVisible(head), wide);
        for (QObject *row : rows) {
            const bool noted = !row->property("description").toString().isEmpty();
            QObject *beside = row->findChild<QObject *>(u"descriptionCell"_s);
            QObject *under = row->findChild<QObject *>(u"descriptionText"_s);
            QCOMPARE(row->property("descriptionWidth").toReal() > 0, wide);
            QCOMPARE(isVisible(beside), wide);
            QCOMPARE(isVisible(under), !wide && noted);
            if (wide) {
                QCOMPARE(row->property("descriptionWidth"), head->property("descriptionWidth"));
                QCOMPARE(row->property("controlsWidth"), head->property("controlsWidth"));
                QCOMPARE(row->property("controlsWidth"), rows.first()->property("controlsWidth"));
            }
        }
    }

    void weightsTable()
    {
        Window w;
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(pageAt(w, u"weightsAction"_s, 1000));
        // the cards stay as they were, in as many columns as fit
        checkTable(w, u"weight:"_s, u"weightsHeader"_s, false);
        QCOMPARE(w.page()->property("maxColumns").toInt(), 3);

        w.window->resize(1500, 900);
        QTRY_VERIFY(w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        checkTable(w, u"weight:"_s, u"weightsHeader"_s, true);
        // the table has the page's width to itself
        QCOMPARE(w.page()->property("maxColumns").toInt(), 1);
        QCOMPARE(text(w.find(u"weightsHeader"_s)->findChild<QObject *>(u"firstLabel"_s)), u"Stat"_s);
        QCOMPARE(text(w.find(u"weightsHeader"_s)->findChild<QObject *>(u"secondLabel"_s)), u"Note"_s);
        QCOMPARE(text(w.find(u"weightsHeader"_s)->findChild<QObject *>(u"thirdLabel"_s)), u"Weight"_s);
        // and is edited as it was
        QVERIFY(setValue(w.find(u"weight:# to maximum Life"_s)->findChild<QObject *>(u"weightBox"_s), 2500,
                         "valueModified"));
        QCOMPARE(w.app->character().character().weights.value(u"# to maximum Life"_s), 2.5);

        w.window->resize(1000, 700);
        QTRY_VERIFY(!w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        checkTable(w, u"weight:"_s, u"weightsHeader"_s, false);
    }

    void weightsTableHasNoHeaderWithoutRows()
    {
        Window w;
        QVERIFY(pageAt(w, u"weightsAction"_s, 1500));
        QVERIFY(isVisible(w.find(u"weightsHeader"_s)));
        // none to have a header over
        w.play({});
        for (const QString &key : w.app->statWeights()->keys())
            w.app->statWeights()->remove(key);
        QTRY_VERIFY(!isVisible(w.find(u"weightsHeader"_s)));
        QVERIFY(isVisible(w.find(u"noWeightsText"_s)));
    }

    void gearGradeTable()
    {
        Window w;
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(pageAt(w, u"gearScoreAction"_s, 1000));
        checkTable(w, u"stat:"_s, u"statsHeader"_s, false);
        QCOMPARE(w.page()->property("maxColumns").toInt(), 3);

        w.window->resize(1500, 900);
        QTRY_VERIFY(w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        checkTable(w, u"stat:"_s, u"statsHeader"_s, true);
        QCOMPARE(w.page()->property("maxColumns").toInt(), 1);
        QCOMPARE(text(w.find(u"statsHeader"_s)->findChild<QObject *>(u"thirdLabel"_s)), u"How much"_s);
        // the choices are still there to make
        QObject *box = w.find(u"stat:Life"_s)->findChild<QObject *>(u"levelBox"_s);
        QVERIFY(isVisible(box));
        QVERIFY(isVisible(w.find(u"weightsOnlySwitch"_s)));

        w.window->resize(1000, 700);
        QTRY_VERIFY(!w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        checkTable(w, u"stat:"_s, u"statsHeader"_s, false);
    }

    void ignoredStatsTables()
    {
        Window w;
        w.play({});
        w.app->ignoredStats()->add(u"#% increased Rarity of Items found"_s);
        QVERIFY(pageAt(w, u"ignoredAction"_s, 1000));
        checkTable(w, u"group:"_s, u"groupsHeader"_s, false);
        checkTable(w, u"ignored:"_s, u"singleHeader"_s, false);
        QCOMPARE(w.page()->property("maxColumns").toInt(), 3);

        w.window->resize(1500, 900);
        QTRY_VERIFY(w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        checkTable(w, u"group:"_s, u"groupsHeader"_s, true);
        checkTable(w, u"ignored:"_s, u"singleHeader"_s, true);
        QCOMPARE(w.page()->property("maxColumns").toInt(), 1);
        QVERIFY(isVisible(w.find(u"ignoreStatButton"_s)));

        w.window->resize(1000, 700);
        QTRY_VERIFY(!w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        checkTable(w, u"group:"_s, u"groupsHeader"_s, false);
        checkTable(w, u"ignored:"_s, u"singleHeader"_s, false);
    }

    void ignoredStatsHeaderNeedsRows()
    {
        // the second table has nothing to be a header over
        Window w;
        QVERIFY(pageAt(w, u"ignoredAction"_s, 1500));
        QVERIFY(isVisible(w.find(u"groupsHeader"_s)));
        QVERIFY(!isVisible(w.find(u"singleHeader"_s)));
        QVERIFY(isVisible(w.find(u"noSingleText"_s)));
    }

    void charactersBesidePlayed()
    {
        Window w;
        w.app->characterList()->add(u"Ash"_s);
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(pageAt(w, u"charactersAction"_s, 1000));
        // a column, with the buttons in each row
        QVERIFY(!isVisible(w.find(u"characterPanel"_s)));
        QVERIFY(!isVisible(w.find(u"playedHeader"_s)));
        QObject *row = w.find(u"character:Wren"_s);
        QVERIFY(isVisible(row->findChild<QObject *>(u"gearButton"_s)));
        QVERIFY(isVisible(row->findChild<QObject *>(u"renameButton"_s)));
        QVERIFY(isVisible(row->findChild<QObject *>(u"removeButton"_s)));

        w.window->resize(1500, 900);
        QTRY_VERIFY(w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        QObject *panel = w.find(u"characterPanel"_s);
        QVERIFY(isVisible(panel));
        QVERIFY(isVisible(w.find(u"playedHeader"_s)));
        // the one you play, with what it goes by
        QCOMPARE(text(w.find(u"panelCharacter"_s)), u"Wren"_s);
        QCOMPARE(text(w.find(u"panelBuild"_s)), u"No build imported"_s);
        QVERIFY(isVisible(w.find(u"panelScore"_s)));
        QCOMPARE(text(w.find(u"panelProfile"_s)), w.app->gearScoring()->state());
        QVERIFY(!text(w.find(u"panelScoring"_s)).isEmpty());
        // beside the list, which has no buttons now
        const auto *list = qobject_cast<QQuickItem *>(row);
        const auto *beside = qobject_cast<QQuickItem *>(panel);
        // once the page has moved the sections into their columns
        QTRY_VERIFY(beside->mapToScene(QPointF(0, 0)).x() >= list->mapToScene(QPointF(list->width(), 0)).x());
        QVERIFY(!isVisible(row->findChild<QObject *>(u"gearButton"_s)));
        QVERIFY(!isVisible(row->findChild<QObject *>(u"renameButton"_s)));
        QVERIFY(!isVisible(row->findChild<QObject *>(u"removeButton"_s)));

        // and back
        w.window->resize(1000, 700);
        QTRY_VERIFY(!w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        QVERIFY(!isVisible(w.find(u"characterPanel"_s)));
        QVERIFY(isVisible(row->findChild<QObject *>(u"gearButton"_s)));
    }

    void characterPanelFollowsTheCharacter()
    {
        Window w;
        QVERIFY(pageAt(w, u"charactersAction"_s, 1500));
        // nobody to show
        QVERIFY(!isVisible(w.find(u"characterPanel"_s)));
        QVERIFY(!isVisible(w.find(u"playedHeader"_s)));

        w.app->characterList()->add(u"Wren"_s);
        QTRY_VERIFY(isVisible(w.find(u"characterPanel"_s)));
        QCOMPARE(text(w.find(u"panelCharacter"_s)), u"Wren"_s);
        w.app->characterList()->add(u"Ash"_s);
        QTRY_COMPARE(text(w.find(u"panelCharacter"_s)), u"Ash"_s);
        w.app->buildImporter()->importText(ranger);
        QTRY_VERIFY(text(w.find(u"panelBuild"_s)).startsWith(u"Level 40 Ranger"_s));
    }

    void characterPanelButtons()
    {
        Window w;
        const QString wren = w.app->characterList()->add(u"Wren"_s);
        QVERIFY(pageAt(w, u"charactersAction"_s, 1500));

        // each says whose it is, as the row's do
        QCOMPARE(spoken(w.find(u"panelGearButton"_s)), u"View Gear of Wren"_s);
        QCOMPARE(spoken(w.find(u"panelRenameButton"_s)), u"Rename Wren"_s);
        QCOMPARE(spoken(w.find(u"panelRemoveButton"_s)), u"Remove Wren"_s);

        QVERIFY(click(w.find(u"panelRenameButton"_s)));
        QObject *rename = w.find(u"renameDialog"_s);
        QVERIFY(isVisible(rename));
        QCOMPARE(text(w.find(u"renameField"_s)), u"Wren"_s);
        QVERIFY(click(button(rename, u"Cancel"_s)));
        QTRY_VERIFY(!isVisible(rename));

        QVERIFY(click(w.find(u"panelRemoveButton"_s)));
        QObject *remove = w.find(u"removeDialog"_s);
        QVERIFY(isVisible(remove));
        QCOMPARE(remove->property("title").toString(), u"Remove Wren?"_s);
        QVERIFY(click(button(remove, u"Cancel"_s)));
        QTRY_VERIFY(!isVisible(remove));
        QCOMPARE(w.app->characterList()->rowCount(), 1);

        QVERIFY(click(w.find(u"panelGearButton"_s)));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QCOMPARE(w.app->characterList()->current(), wren);
    }

    void itemHistoryWithTheItem()
    {
        Window w;
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        w.app->history().add(item(itemtexts::helmetOld));
        w.app->history().add(item(itemtexts::advancedRing));
        w.app->history().add(item(itemtexts::helmetNew));
        QVERIFY(pageAt(w, u"checkedAction"_s, 1000));
        // narrow it is the list, as it was
        QVERIFY(!isVisible(w.find(u"itemPanel"_s)));
        QVERIFY(!isVisible(w.find(u"detailHeader"_s)));
        QCOMPARE(w.page()->property("maxColumns").toInt(), 3);
        QVERIFY(isVisible(w.find(u"checked:0"_s)->findChild<QObject *>(u"checkAgainButton"_s)));

        w.window->resize(1500, 900);
        QTRY_VERIFY(w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        QVERIFY(isVisible(w.find(u"itemPanel"_s)));
        QVERIFY(isVisible(w.find(u"detailHeader"_s)));
        QCOMPARE(w.page()->property("maxColumns").toInt(), 2);
        // the newest is shown to start with
        QCOMPARE(text(w.find(u"detailName"_s)), u"Doom Visage"_s);
        QCOMPARE(text(w.find(u"detailKind"_s)), u"Rare, Expert Visored Helm, Helmets"_s);
        QVERIFY(text(w.find(u"detailLevels"_s)).startsWith(u"Item level 79"_s));
        QVERIFY(!isVisible(w.find(u"detailCorrupted"_s)));
        QVERIFY(isVisible(w.find(u"detailProperties"_s)));
        QVERIFY(isVisible(w.find(u"detailMods"_s)));
        QCOMPARE(w.page()->property("picked").toInt(), 0);
        // beside the list
        const auto *panel = qobject_cast<QQuickItem *>(w.find(u"itemPanel"_s));
        const auto *list = qobject_cast<QQuickItem *>(w.find(u"checked:0"_s));
        QTRY_VERIFY(panel->mapToScene(QPointF(0, 0)).x() >= list->mapToScene(QPointF(list->width(), 0)).x());

        // a row picks its item to show
        QVERIFY(click(w.find(u"checked:1"_s)));
        QCOMPARE(w.page()->property("picked").toInt(), 1);
        QCOMPARE(text(w.find(u"detailName"_s)), u"Storm Loop"_s);
        QVERIFY(click(w.find(u"checked:2"_s)));
        QCOMPARE(text(w.find(u"detailName"_s)), u"Grim Crown"_s);
        // and a row takes the keyboard focus, as a row of a list does
        QCOMPARE(w.find(u"checked:2"_s)->property("focusPolicy").toInt(), int(Qt::StrongFocus));

        w.window->resize(1000, 700);
        QTRY_VERIFY(!w.page()->property("wide").toBool());
        QVERIFY(w.settled());
        QVERIFY(!isVisible(w.find(u"itemPanel"_s)));
        QCOMPARE(w.find(u"checked:2"_s)->property("focusPolicy").toInt(), int(Qt::NoFocus));
    }

    void itemHistoryPanelChecksAgain()
    {
        Window w;
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        w.app->history().add(item(itemtexts::helmetNew));
        w.app->history().add(item(itemtexts::advancedRing));
        QVERIFY(pageAt(w, u"checkedAction"_s, 1500));
        QVERIFY(click(w.find(u"checked:1"_s)));
        QCOMPARE(text(w.find(u"detailName"_s)), u"Doom Visage"_s);
        ItemPopup &popup = w.app->checker().popup();
        QVERIFY(!popup.isVisible());

        // the one it shows, which goes to the top
        QVERIFY(click(w.find(u"detailCheckAgain"_s)));
        QVERIFY(popup.isVisible());
        QCOMPARE(popup.report().name, u"Doom Visage"_s);
        QCOMPARE(text(w.find(u"checked:0"_s)), u"Doom Visage"_s);
        QCOMPARE(text(w.find(u"detailName"_s)), u"Doom Visage"_s);
        QCOMPARE(w.page()->property("picked").toInt(), 0);
    }

    void itemHistoryPanelFollowsTheList()
    {
        Window w;
        QVERIFY(pageAt(w, u"checkedAction"_s, 1500));
        // nothing to show yet
        QVERIFY(!isVisible(w.find(u"itemPanel"_s)));
        QVERIFY(!isVisible(w.find(u"detailHeader"_s)));
        QVERIFY(isVisible(w.find(u"noItemsText"_s)));

        w.app->history().add(item(itemtexts::helmetOld));
        QTRY_VERIFY(isVisible(w.find(u"itemPanel"_s)));
        QCOMPARE(text(w.find(u"detailName"_s)), u"Grim Crown"_s);
        w.app->history().add(item(itemtexts::advancedRing));
        QTRY_COMPARE(text(w.find(u"detailName"_s)), u"Storm Loop"_s);

        w.app->history().clearRecent();
        QTRY_VERIFY(!isVisible(w.find(u"itemPanel"_s)));
        QVERIFY(!isVisible(w.find(u"detailHeader"_s)));
    }

    void itemHistoryPanelStaysInView()
    {
        Window w;
        for (int n = 1; n <= 25; ++n) {
            w.app->history().add(item(u"Item Class: Rings\nRarity: Rare\nRing %1\nGold Ring\n--------\n"
                                      "Item Level: 70\n--------\n+%1 to maximum Life"_s.arg(n)));
        }
        QVERIFY(pageAt(w, u"checkedAction"_s, 1500));
        auto *flick = w.page()->property("flickable").value<QQuickItem *>();
        QVERIFY(flick);
        auto *card = qobject_cast<QQuickItem *>(w.find(u"detailName"_s));
        QVERIFY(flick->property("contentHeight").toReal() > flick->height() + 400);
        const auto top = [&] {
            return card->mapToScene(QPointF(0, 0)).y();
        };
        const qreal start = top();
        QVERIFY(start >= 0);

        // scrolled down the list, it comes along
        flick->setProperty("contentY", 400);
        QTRY_VERIFY(top() >= 0 && top() < flick->height());
        // and goes back with it
        flick->setProperty("contentY", 0);
        QTRY_COMPARE(top(), start);
    }
};

QTEST_MAIN(TestMainWindowPages)
#include "tst_mainwindow_pages.moc"
