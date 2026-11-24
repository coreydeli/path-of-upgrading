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

class TestMainWindow : public QObject
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

    // The gear page on a window wide enough for the equipment screen.
    static bool wide(Window &w)
    {
        if (!w.show(u"gearAction"_s) || !w.shown())
            return false;
        w.window->resize(1500, 900);
        return QTest::qWaitFor([&] { return w.settled() && isVisible(w.find(u"equipment"_s)); }, 2000);
    }

    static QPointF where(Window &w, const QString &name)
    {
        const auto *item = qobject_cast<QQuickItem *>(w.find(name));
        return item ? item->mapToScene(QPointF(0, 0)) : QPointF(-1, -1);
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

    void generalName()
    {
        Window w;
        // the sidebar entry and the page both say General, apart from the Settings group they're in
        QCOMPARE(text(w.window->findChild<QObject *>(u"overlayAction"_s)), u"General"_s);
        QCOMPARE(text(w.window->findChild<QObject *>(u"tradeAction"_s)), u"Trade"_s);
        QCOMPARE(text(w.window->findChild<QObject *>(u"pobAction"_s)), u"Build Review"_s);
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"General"_s);
    }

    void buildReviewName()
    {
        Window w;
        // the sidebar entry and the page say Build Review, and the program keeps its own name inside
        QCOMPARE(text(w.window->findChild<QObject *>(u"pobAction"_s)), u"Build Review"_s);
        QVERIFY(w.show(u"pobAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Build Review"_s);
        QCOMPARE(text(w.find(u"analysisIntro"_s)),
                 u"Path of Building works out what an item would do to your build's damage and defences."_s);
        QCOMPARE(text(w.find(u"enabledSwitch"_s)), u"Judge items with Path of Building"_s);
    }

    void buildReviewTopsGearSettings()
    {
        Window w;
        QVERIFY(w.shown());
        // not a row of its own at the top, but the first in Gear Settings
        QTRY_VERIFY(w.sidebarRow(u"Gear Settings"_s));
        QVERIFY(!w.sidebarRow(u"Build Review"_s));
        QVERIFY(w.clickRow(u"Gear Settings"_s));
        QTRY_VERIFY(w.sidebarRow(u"Build Review"_s));
        // the top level's rows linger while the group slides in, Your Gear where Build Review goes
        QTRY_VERIFY(!w.sidebarRow(u"Item History"_s));
        QTRY_VERIFY(!w.sidebarRow(u"Your Gear"_s));
        const auto top = [&](const QString &text) {
            QQuickItem *row = w.sidebarRow(text);
            return row ? row->mapToScene(QPointF(0, 0)).y() : -1.0;
        };
        QVERIFY(top(u"Build Review"_s) > top(u"Back"_s));
        QVERIFY(top(u"Build Review"_s) < top(u"Characters"_s));
        // and the sidebar stays there while it's shown
        QVERIFY(w.clickRow(u"Build Review"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Build Review"_s);
        QTest::qWait(500);
        QVERIFY(w.sidebarRow(u"Back"_s));
        QCOMPARE(w.sidebarRow(u"Build Review"_s)->property("checked").toBool(), true);
    }

    void itemHistoryName()
    {
        Window w;
        // the sidebar entry
        QCOMPARE(text(w.window->findChild<QObject *>(u"checkedAction"_s)), u"Item History"_s);
        QVERIFY(w.show(u"checkedAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Item History"_s);
        // it says what it is, also before there is anything in it
        QObject *intro = w.find(u"historyIntro"_s);
        QVERIFY(isVisible(intro));
        QCOMPARE(text(intro),
                 u"Items you check in the game land here, newest first, up to the last "
                 "30. That covers the hotkey and, while \"Also check gear you copy yourself\" is "
                 "on, gear you copy by hand. When the list is full, the oldest one drops off."_s);
        QVERIFY(isVisible(w.find(u"noItemsText"_s)));
    }

    void checkedItems()
    {
        Window w;
        // checked while you were on another page
        w.app->history().add(item(itemtexts::helmetOld));
        w.app->history().add(item(itemtexts::advancedRing));
        QVERIFY(w.show(u"checkedAction"_s));
        QVERIFY(!isVisible(w.find(u"noItemsText"_s)));
        QVERIFY(isVisible(w.find(u"forgetSeparator"_s)));
        QVERIFY(isVisible(w.find(u"forgetButton"_s)));
        QVERIFY(isVisible(w.find(u"checkedHelp"_s)));

        // the newest first
        QObject *ring = w.find(u"checked:0"_s);
        QVERIFY(isVisible(ring));
        QCOMPARE(text(ring), u"Storm Loop"_s);
        QCOMPARE(ring->property("description").toString(), u"Rare, Sapphire Ring, Rings, item level 75"_s);
        QObject *again = ring->findChild<QObject *>(u"checkAgainButton"_s);
        QVERIFY(isVisible(again));
        // the buttons look alike, so each says which item it checks
        const QAccessibleInterface *reads = QAccessible::queryAccessibleInterface(again);
        QVERIFY(reads);
        QCOMPARE(reads->text(QAccessible::Name), u"Check Storm Loop Again"_s);
        QObject *helmet = w.find(u"checked:1"_s);
        QCOMPARE(text(helmet), u"Grim Crown"_s);
        QCOMPARE(helmet->property("description").toString(), u"Rare, Visored Helm, Helmets, item level 50"_s);
        QVERIFY(!search(w.page(), [](QObject *o) { return o->objectName() == u"checked:2"; }));

        // and one more at the front
        w.app->history().add(item(itemtexts::helmetNew));
        QCOMPARE(text(w.find(u"checked:0"_s)), u"Doom Visage"_s);
        QCOMPARE(text(w.find(u"checked:2"_s)), u"Grim Crown"_s);
    }

    void checkedItemLevels()
    {
        // an item without a level says what it needs, and a magic one has no base type
        Window w;
        QVERIFY(w.show(u"checkedAction"_s));
        w.app->history().add(item(u"Item Class: Boots\nRarity: Magic\nSturdy Leather Boots of the Fox\n"
                                  "--------\nRequirements:\nLevel: 12\n--------\n+10 to Dexterity"_s));
        QObject *boots = w.find(u"checked:0"_s);
        QCOMPARE(text(boots), u"Sturdy Leather Boots of the Fox"_s);
        QCOMPARE(boots->property("description").toString(), u"Magic, Boots, needs level 12"_s);

        w.app->history().add(item(u"Item Class: Belts\nRarity: Normal\nLeather Belt\n--------\n"
                                  "+5 to Strength"_s));
        QObject *belt = w.find(u"checked:0"_s);
        QCOMPARE(text(belt), u"Leather Belt"_s);
        QCOMPARE(belt->property("description").toString(), u"Normal, Belts"_s);
    }

    void checkAnItemAgain()
    {
        Window w;
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(w.show(u"checkedAction"_s));
        w.app->history().add(item(itemtexts::helmetNew));
        w.app->history().add(item(itemtexts::advancedRing));
        ItemPopup &popup = w.app->checker().popup();
        QVERIFY(!popup.isVisible());

        QVERIFY(click(w.find(u"checked:1"_s)->findChild<QObject *>(u"checkAgainButton"_s)));
        QVERIFY(popup.isVisible());
        QCOMPARE(popup.report().name, u"Doom Visage"_s);
        QCOMPARE(popup.report().versus, u"vs Grim Crown"_s);
        // which is the one you checked last now
        QCOMPARE(text(w.find(u"checked:0"_s)), u"Doom Visage"_s);
        QCOMPARE(text(w.find(u"checked:1"_s)), u"Storm Loop"_s);
        QCOMPARE(w.app->history().recent().size(), 2);
    }

    void forgetCheckedItems()
    {
        Window w;
        QVERIFY(w.show(u"checkedAction"_s));
        w.app->history().add(item(itemtexts::helmetOld));
        const SeenStats seen = w.app->history().seenStats();
        QVERIFY(!seen.isEmpty());

        QVERIFY(click(w.find(u"forgetButton"_s)));
        QObject *dialog = w.find(u"forgetDialog"_s);
        QVERIFY(isVisible(dialog));
        QCOMPARE(dialog->property("title").toString(), u"Forget the Items?"_s);
        // nothing yet
        QCOMPARE(w.app->history().recent().size(), 1);

        QVERIFY(click(button(dialog, u"Forget Them"_s)));
        QCOMPARE(w.app->history().recent().size(), 0);
        QVERIFY(isVisible(w.find(u"noItemsText"_s)));
        QVERIFY(!isVisible(w.find(u"forgetSeparator"_s)));
        QVERIFY(!isVisible(w.find(u"forgetButton"_s)));
        QVERIFY(!isVisible(w.find(u"checkedHelp"_s)));
        QTRY_VERIFY(!isVisible(dialog));
        QTRY_VERIFY(!search(w.page(), [](QObject *o) { return o->objectName() == u"checked:0"; }));
        // and the stat picker still has their stats
        QCOMPARE(w.app->history().seenStats(), seen);
    }

    void keepCheckedItems()
    {
        Window w;
        QVERIFY(w.show(u"checkedAction"_s));
        w.app->history().add(item(itemtexts::helmetOld));
        QVERIFY(click(w.find(u"forgetButton"_s)));
        QObject *dialog = w.find(u"forgetDialog"_s);
        QVERIFY(click(button(dialog, u"Cancel"_s)));
        QCOMPARE(w.app->history().recent().size(), 1);
        QVERIFY(!isVisible(w.find(u"noItemsText"_s)));
        QCOMPARE(text(w.find(u"checked:0"_s)), u"Grim Crown"_s);
        QTRY_VERIFY(!isVisible(dialog));
    }

    void league()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QObject *leagues = w.find(u"leagueBox"_s);
        // the current one, then the site's without their hardcore ones
        QCOMPARE(leagues->property("count").toInt(), 3);
        QCOMPARE(leagues->property("currentIndex").toInt(), 0);
        QCOMPARE(leagues->property("currentText").toString(), u"The current league"_s);
        QCOMPARE(w.find(u"searchedText"_s)->property("description").toString(), u"Runes of Aldur"_s);

        QVERIFY(activate(leagues, 2));
        QCOMPARE(w.settings().trade.league.name, u"Standard"_s);
        QVERIFY(toggle(w.find(u"hardcoreSwitch"_s)));
        QCOMPARE(w.settings().trade.league.hardcore, true);
        QCOMPARE(w.find(u"searchedText"_s)->property("description").toString(), u"Hardcore"_s);
    }

    void soloSelfFound()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.find(u"unavailableText"_s)->property("visible").toBool(), false);
        QVERIFY(toggle(w.find(u"soloSwitch"_s)));
        QCOMPARE(w.settings().trade.league.soloSelfFound, true);
        QObject *unavailable = w.find(u"unavailableText"_s);
        QCOMPARE(unavailable->property("visible").toBool(), true);
        QCOMPARE(text(unavailable), u"There's no trading in solo self-found, so trade searches are off."_s);
        QCOMPARE(w.find(u"searchedText"_s)->property("visible").toBool(), false);
        QVERIFY(!w.app->checker().popup().isTradeAvailable());
    }

    void showsTradeSettings()
    {
        Window w;
        QVERIFY(w.rated());
        Settings settings = w.settings();
        settings.trade.league = {.name = u"Standard"_s, .hardcore = true};
        settings.trade.status = ListingStatus::Online;
        settings.trade.budget.max = 3;
        QVERIFY(w.app->settings().set(settings));
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.find(u"leagueBox"_s)->property("currentText").toString(), u"Standard"_s);
        QCOMPARE(w.find(u"hardcoreSwitch"_s)->property("checked").toBool(), true);
        QCOMPARE(w.find(u"sellersBox"_s)->property("currentText").toString(), u"In person, while online"_s);
        QCOMPARE(w.spinBox(u"budgetMax"_s)->property("value").toInt(), 30);
    }

    void tradeOff()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(toggle(w.find(u"enabledSwitch"_s)));
        QCOMPARE(w.settings().trade.enabled, false);
        // and the page says why, and how to turn it on again
        QObject *unavailable = w.find(u"unavailableText"_s);
        QCOMPARE(unavailable->property("visible").toBool(), true);
        QCOMPARE(text(unavailable), tradeUnavailable({.enabled = false}));
        QVERIFY(text(unavailable).contains(u"Search the trade site"_s));
        QVERIFY(!w.app->checker().popup().isTradeAvailable());
        QCOMPARE(w.app->checker().popup().tradeNote(), text(unavailable));

        QVERIFY(toggle(w.find(u"enabledSwitch"_s)));
        QCOMPARE(w.find(u"unavailableText"_s)->property("visible").toBool(), false);
        QVERIFY(w.app->checker().popup().tradeNote().isEmpty());
    }

    void releaseSidebarHasNoTrade()
    {
        Window release(true, false, {}, used(), false);
        QVERIFY(release.window->findChild<QObject *>(u"tradeAction"_s));
        QVERIFY(!isVisible(release.window->findChild<QObject *>(u"tradeAction"_s)));
        QVERIFY(!release.app->tradeOffered());
        QVERIFY(release.settings().trade.enabled);
        QVERIFY(release.app->checker().popup().tradeNote().isEmpty());
        QVERIFY(!release.app->checker().popup().isTradeAvailable());
        QVERIFY(release.network->sent.isEmpty());
    }

    void betaSettingsHaveTheStartOverCard()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(isVisible(w.find(u"startOverCard"_s)));
        QVERIFY(isVisible(w.find(u"startOverButton"_s)));
    }

    void releaseSettingsHaveNoStartOverCard()
    {
        Window release(true, false, {}, used(), true, nullptr, true, true, false);
        QVERIFY(!release.app->startOver()->isOffered());
        QVERIFY(release.show(u"overlayAction"_s));
        QVERIFY(isVisible(release.find(u"checkKey"_s)));
        QVERIFY(!isVisible(release.find(u"startOverCard"_s)));
        QVERIFY(!isVisible(release.find(u"startOverButton"_s)));
    }

    void startOverAsksFirst()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        w.app->startOver()->setRestart([] { return true; });
        QObject *dialog = w.window->findChild<QObject *>(u"startOverDialog"_s);
        QVERIFY(dialog);
        QVERIFY(!dialog->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(w.find(u"startOverButton"_s), "clicked"));
        QVERIFY(QTest::qWaitFor([&] { return dialog->property("opened").toBool(); }, 5000));
        // nothing is wiped until it's said yes to
        QVERIFY(!w.app->startOver()->isBusy());
        QVERIFY(QFileInfo::exists(w.dir.filePath(u"settings.conf"_s)));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
    }

    void sidebarHasTradeWhenOffered()
    {
        Window w;
        QVERIFY(w.app->tradeOffered());
        QVERIFY(isVisible(w.window->findChild<QObject *>(u"tradeAction"_s)));
    }

    void releaseSettingsHaveNoResultsKey()
    {
        Window release(true, false, {}, used(), false);
        QVERIFY(release.show(u"overlayAction"_s));
        QVERIFY(isVisible(release.find(u"checkKey"_s)));
        QVERIFY(!isVisible(release.find(u"resultsKey"_s)));
        QVERIFY(!release.shortcuts->names.contains(u"trade-results"_s));
    }

    void releaseSetupGuideHasTheLeagueButNoTradeStep()
    {
        Window w(true, false, {}, std::nullopt, false);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        QCOMPARE(guide->property("steps").toStringList(),
                 (QStringList{u"welcome"_s, u"league"_s, u"character"_s, u"look"_s, u"done"_s}));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(guide->property("title").toString(), u"Pick Your League"_s);
        QVERIFY(isVisible(w.inGuide(u"leagueStep"_s)));
        // solo self-found, with or without hardcore, has nothing to turn off here
        QObject *solo = w.inGuide(u"setupSoloSwitch"_s);
        QCOMPARE(solo->property("description").toString(), u"For characters that play without trading."_s);
        QVERIFY(toggle(w.inGuide(u"setupHardcoreSwitch"_s)));
        QVERIFY(toggle(solo));
        QVERIFY(w.settings().trade.league.hardcore);
        QVERIFY(w.settings().trade.league.soloSelfFound);
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(guide->property("title").toString(), u"Your Character"_s);
        QVERIFY(!isVisible(w.inGuide(u"tradeStep"_s)));
        QVERIFY(w.network->sent.isEmpty());
    }

    void setupGuideSoloSelfFoundTurnsTradeOff()
    {
        Window w(true, false, {}, std::nullopt);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        // trade is on, which a first start only has in a beta build
        Settings on = w.settings();
        on.trade.enabled = true;
        QVERIFY(w.app->settings().set(on));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(w.app->trade().unavailable().isEmpty());
        QVERIFY(toggle(w.inGuide(u"setupHardcoreSwitch"_s)));
        QVERIFY(toggle(w.inGuide(u"setupSoloSwitch"_s)));
        QVERIFY(!w.app->trade().unavailable().isEmpty());
        QVERIFY(!w.app->checker().popup().isTradeAvailable());
        QCOMPARE(guide->property("steps").toStringList(),
                 (QStringList{u"welcome"_s, u"league"_s, u"character"_s, u"look"_s, u"done"_s}));
        QVERIFY(w.inGuide(u"setupSoloSwitch"_s)
                    ->property("description")
                    .toString()
                    .contains(u"turns trade searches off"_s));
    }

    void releaseSettingsHaveTheLeague()
    {
        Window w(true, false, {}, used(), false);
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(isVisible(w.find(u"leagueBox"_s)));
        QVERIFY(toggle(w.find(u"hardcoreSwitch"_s)));
        QVERIFY(toggle(w.find(u"soloSwitch"_s)));
        QVERIFY(w.settings().trade.league.hardcore);
        QVERIFY(w.settings().trade.league.soloSelfFound);
        // as the setup guide has them
        QVERIFY(w.network->sent.isEmpty());
    }

    void settingsLeagueIsOnTheTradePageWhenOffered()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(!isVisible(w.find(u"leagueBox"_s)));
    }

    void cardsInColumns()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.shown());
        const auto pos = [&](const QString &name) {
            const auto *item = qobject_cast<QQuickItem *>(w.find(name));
            return item ? item->mapToScene(QPointF(0, 0)) : QPointF(-1, -1);
        };

        // narrow, they're in one column, in the order they're written
        w.window->resize(700, 1400);
        QTRY_VERIFY(w.settled() && pos(u"leagueBox"_s).x() == pos(u"budgetMin"_s).x());
        QCOMPARE(pos(u"sellersBox"_s).x(), pos(u"leagueBox"_s).x());
        QCOMPARE(pos(u"currencyBox"_s).x(), pos(u"leagueBox"_s).x());
        QVERIFY(pos(u"leagueBox"_s).y() < pos(u"sellersBox"_s).y());
        QVERIFY(pos(u"sellersBox"_s).y() < pos(u"budgetMin"_s).y());
        QVERIFY(pos(u"budgetMin"_s).y() < pos(u"sessionField"_s).y());

        // wide, they're beside each other
        w.window->resize(1900, 900);
        QTRY_VERIFY(w.settled() && pos(u"leagueBox"_s).x() > pos(u"budgetMin"_s).x());
        QVERIFY(pos(u"sellersBox"_s).x() > pos(u"leagueBox"_s).x());
        QVERIFY(pos(u"sessionField"_s).x() > pos(u"leagueBox"_s).x());

        // and narrow again, they're back in the order they were written in
        w.window->resize(700, 1400);
        QTRY_VERIFY(w.settled() && pos(u"leagueBox"_s).x() == pos(u"budgetMin"_s).x());
        QVERIFY(pos(u"leagueBox"_s).y() < pos(u"sellersBox"_s).y());
        QVERIFY(pos(u"sellersBox"_s).y() < pos(u"budgetMin"_s).y());
        QVERIFY(pos(u"budgetMin"_s).y() < pos(u"sessionField"_s).y());
    }

    // Qt 6.8 warns if a drawer turns modal while it's open, so the sidebar closes first
    void sidebarClosesBeforeTurningModal()
    {
        Window w;
        QVERIFY(w.shown());
        const auto *drawer = w.window->property("globalDrawer").value<QObject *>();
        QVERIFY(drawer);
        const auto modal = [&] {
            return drawer->property("modal").toBool();
        };
        const auto open = [&] {
            return drawer->property("visible").toBool();
        };
        const auto settledOpen = [&] {
            return !modal() && open() && drawer->property("position").toDouble() == 1.0;
        };

        w.window->resize(1900, 900);
        QTRY_VERIFY(settledOpen());

        // it's shut by the time it's modal
        w.window->resize(450, 1400);
        QTRY_VERIFY(modal());
        QVERIFY(!open());

        w.window->resize(1900, 900);
        QTRY_VERIFY(settledOpen());

        // and a window that's wide again while the sidebar closes keeps it
        w.window->resize(450, 1400);
        QTRY_VERIFY(drawer->property("animating").toBool());
        w.window->resize(1900, 900);
        QTRY_VERIFY(settledOpen());
    }

    void budgetLabels()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.find(u"budgetMin"_s)->property("label").toString(), u"Lowest price"_s);
        QCOMPARE(w.find(u"budgetMax"_s)->property("label").toString(), u"Highest price"_s);
        QCOMPARE(w.find(u"budgetMin"_s)->property("description").toString(),
                 u"Anything cheaper won't show up."_s);
        QCOMPARE(w.find(u"budgetMax"_s)->property("description").toString(),
                 u"Anything pricier won't show up."_s);
        QCOMPARE(w.find(u"currencyBox"_s)->property("text").toString(), u"Currency"_s);
    }

    void ratePricesLabels()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QObject *autoRate = w.find(u"autoRateSwitch"_s);
        QCOMPARE(autoRate->property("text").toString(), u"Use poe.ninja's currency prices"_s);
        QCOMPARE(autoRate->property("description").toString(),
                 u"poe.ninja is a site that tracks what currency trades for in each league. Its price for a "
                 u"Divine Orb turns your budget into one currency. Right now a Divine Orb is 400 Exalted "
                 u"Orbs in Runes of Aldur."_s);
        QCOMPARE(w.find(u"rateMinutes"_s)->property("label").toString(),
                 u"Update poe.ninja's prices automatically"_s);
        QCOMPARE(w.find(u"rateMinutes"_s)->property("unit").toString(), u"minutes"_s);
        QVERIFY(w.settings().trade.rateMinutes > 1);
        QCOMPARE(w.find(u"divineRate"_s)->property("label").toString(), u"Exalted Orbs to a Divine Orb"_s);
        QCOMPARE(w.find(u"divineRate"_s)->property("description").toString(),
                 u"Used when poe.ninja's prices are off, or it has no price yet."_s);
    }

    void budget()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QObject *max = w.spinBox(u"budgetMax"_s);
        QCOMPARE(max->property("displayText").toString(), u"Any"_s);
        // in tenths
        QVERIFY(setValue(max, 25, "valueModified"));
        QCOMPARE(w.settings().trade.budget.max, 2.5);
        QCOMPARE(max->property("displayText").toString(), QLocale().toString(2.5, 'f', 1));
        QCOMPARE(max->property("stepSize").toInt(), 5);
        QCOMPARE(w.spinBox(u"budgetMin"_s)->property("stepSize").toInt(), 5);
        QCOMPARE(typed(max, QLocale().toString(1.5, 'f', 1)), 15);
        QCOMPARE(typed(max, u"Any"_s), 0);

        QVERIFY(activate(w.find(u"currencyBox"_s), 1));
        QCOMPARE(w.settings().trade.budget.currency, Currency::Exalted);
        QCOMPARE(max->property("stepSize").toInt(), 10);
        QVERIFY(setValue(w.spinBox(u"budgetMin"_s), 200, "valueModified"));
        QCOMPARE(w.settings().trade.budget.min, 20.0);
        QCOMPARE(w.spinBox(u"budgetMin"_s)->property("displayText").toString(), QLocale().toString(20));

        QVERIFY(activate(w.find(u"sellersBox"_s), 3));
        QCOMPARE(w.settings().trade.status, ListingStatus::Any);
    }

    void rate()
    {
        Window w;
        QVERIFY(w.rated());
        QVERIFY(w.show(u"tradeAction"_s));
        QObject *autoRate = w.find(u"autoRateSwitch"_s);
        QVERIFY(autoRate->property("description")
                    .toString()
                    .endsWith(u" Right now a Divine Orb is 400 Exalted Orbs in Runes of Aldur."_s));
        // which goes before yours
        QCOMPARE(w.find(u"divineRate"_s)->property("enabled").toBool(), false);

        // and is asked for again every ten minutes, as it says
        QObject *minutes = w.find(u"rateMinutes"_s);
        QCOMPARE(minutes->property("description").toString(),
                 u"Asks poe.ninja again after this many minutes, from 1 to 60. Prices updated just now."_s);
        QCOMPARE(w.spinBox(u"rateMinutes"_s)->property("value").toInt(), 10);
        QVERIFY(setValue(w.spinBox(u"rateMinutes"_s), 20, "valueModified"));
        QCOMPARE(w.settings().trade.rateMinutes, 20);
        QCOMPARE(minutes->property("unit").toString(), u"minutes"_s);
        QVERIFY(setValue(w.spinBox(u"rateMinutes"_s), 1, "valueModified"));
        QCOMPARE(minutes->property("unit").toString(), u"minute"_s);
        QVERIFY(setValue(w.spinBox(u"rateMinutes"_s), 20, "valueModified"));
        QCOMPARE(w.app->ratesRefresher().waitMinutes(), 20);
        QVERIFY(minutes->property("enabled").toBool());

        QVERIFY(toggle(autoRate));
        QCOMPARE(w.settings().trade.autoRate, false);
        // when it isn't, there's nothing to wait for
        QVERIFY(!minutes->property("enabled").toBool());
        QCOMPARE(minutes->property("description").toString(),
                 u"Asks poe.ninja again after this many minutes, from 1 to 60."_s);
        QCOMPARE(w.find(u"divineRate"_s)->property("enabled").toBool(), true);
        QVERIFY(setValue(w.spinBox(u"divineRate"_s), 3505, "valueModified"));
        QCOMPARE(w.settings().trade.budget.divineRate, 350.5);
    }

    void noRateYet()
    {
        // poe.ninja, which is asked as the app starts, and says nothing
        Window w(true, false, {}, used(), true, nullptr, true, false);
        QVERIFY(w.shown());
        QTRY_VERIFY(w.app->rates().isRefreshing());
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.find(u"autoRateSwitch"_s)
                    ->property("description")
                    .toString()
                    .endsWith(u" It has no price yet."_s));
        QCOMPARE(
            w.find(u"rateMinutes"_s)->property("description").toString(),
            u"Asks poe.ninja again after this many minutes, from 1 to 60. The prices haven't come in yet."_s);
        QCOMPARE(w.find(u"divineRate"_s)->property("enabled").toBool(), true);
    }

    void sessionNotSaved()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"No session ID is saved."_s);
        QVERIFY(!isVisible(w.find(u"sessionError"_s)));
        QVERIFY(text(w.find(u"sessionHelp"_s)).contains(u"system keyring"_s));
        // typed in as a password, with nothing to save or forget
        QCOMPARE(w.find(u"sessionField"_s)->property("echoMode").toInt(), 2);
        QVERIFY(!w.find(u"saveButton"_s)->property("enabled").toBool());
        QVERIFY(!w.find(u"forgetButton"_s)->property("enabled").toBool());
    }

    void findingTheSessionId()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QObject *steps = w.find(u"findSessionSteps"_s);
        QObject *button = w.find(u"findSessionButton"_s);
        QVERIFY(!isVisible(steps));
        QVERIFY(!isVisible(w.find(u"signInButton"_s)));
        QCOMPARE(text(button), u"Where Do I Find It?"_s);

        QVERIFY(click(button));
        QVERIFY(isVisible(steps));
        QCOMPARE(text(button), u"Hide Where to Find It"_s);
        QVERIFY(text(steps).startsWith(u"1. Sign in"_s));
        QVERIFY(text(steps).contains(u"POESESSID"_s));
        QVERIFY(steps->property("description").toString().contains(u"system keyring"_s));
        // nothing opens until you click for it
        QVERIFY(w.opener->opened.isEmpty());
        QVERIFY(click(w.find(u"signInButton"_s)));
        QCOMPARE(w.opener->opened, QList<QUrl>{signInUrl()});

        QVERIFY(click(button));
        QVERIFY(!isVisible(steps));
        QVERIFY(!isVisible(w.find(u"signInButton"_s)));
    }

    void saveSession()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QObject *field = w.find(u"sessionField"_s);
        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(w.find(u"saveButton"_s)->property("enabled").toBool());
        QVERIFY(click(w.find(u"saveButton"_s)));
        // while the keyring is asked
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"Waiting for the keyring."_s);
        QVERIFY(!field->property("enabled").toBool());
        QVERIFY(!w.find(u"saveButton"_s)->property("enabled").toBool());
        QVERIFY(w.keyringAnswered());

        QCOMPARE(w.keyring->secrets.value(u"POESESSID"_s), fakeId);
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"A session ID is saved."_s);
        QVERIFY(!isVisible(w.find(u"sessionError"_s)));
        // which isn't shown again
        QCOMPARE(text(field), QString());
        QVERIFY(field->property("enabled").toBool());
        QVERIFY(w.find(u"forgetButton"_s)->property("enabled").toBool());
    }

    void saveSessionWithEnter()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QObject *field = w.find(u"sessionField"_s);
        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(QMetaObject::invokeMethod(field, "accepted"));
        QVERIFY(w.keyringAnswered());
        QCOMPARE(w.keyring->secrets.value(u"POESESSID"_s), fakeId);
    }

    void notASessionId()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QObject *field = w.find(u"sessionField"_s);
        QVERIFY(field->setProperty("text", u"not an id!"_s));
        QVERIFY(click(w.find(u"saveButton"_s)));
        QObject *error = w.find(u"sessionError"_s);
        QVERIFY(isVisible(error));
        QCOMPARE(text(error), u"That doesn't look like a session ID. It's only letters and digits."_s);
        QCOMPARE(w.keyring->writes, 0);
        // which stays to be put right
        QCOMPARE(text(field), u"not an id!"_s);

        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(click(w.find(u"saveButton"_s)));
        QVERIFY(w.keyringAnswered());
        QVERIFY(!isVisible(error));
        QCOMPARE(w.keyring->secrets.value(u"POESESSID"_s), fakeId);
    }

    void keyringLocked()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        w.keyring->failure = u"The keyring is locked."_s;
        QObject *field = w.find(u"sessionField"_s);
        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(click(w.find(u"saveButton"_s)));
        QVERIFY(w.keyringAnswered());
        QObject *error = w.find(u"sessionError"_s);
        QVERIFY(isVisible(error));
        QCOMPARE(text(error), u"The keyring is locked."_s);
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"No session ID is saved."_s);
        // so that it can be tried again
        QCOMPARE(text(field), fakeId);
    }

    void forgetSession()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.keyringAnswered());
        QVERIFY(w.find(u"sessionField"_s)->setProperty("text", fakeId));
        QVERIFY(click(w.find(u"saveButton"_s)));
        QVERIFY(w.keyringAnswered());
        QVERIFY(click(w.find(u"forgetButton"_s)));
        QVERIFY(w.keyringAnswered());
        QVERIFY(w.keyring->secrets.isEmpty());
        QCOMPARE(text(w.find(u"sessionStatus"_s)), u"No session ID is saved."_s);
        QVERIFY(!w.find(u"forgetButton"_s)->property("enabled").toBool());
        QVERIFY(!isVisible(w.find(u"sessionError"_s)));
    }

    void checkKey()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *recorder = w.recorder(u"checkKey"_s);
        QCOMPARE(recorder->property("keySequence").value<QKeySequence>(), QKeySequence(Qt::Key_F2));
        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F6)));
        QCOMPARE(w.shortcuts->keys.value(u"check-item"_s), QKeySequence(Qt::Key_F6));
        QCOMPARE(w.settings().overlay.hotkey, u"F6"_s);
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), false);
        QCOMPARE(w.find(u"hotkeysError"_s)->property("visible").toBool(), false);
    }

    void keyTaken()
    {
        Window w;
        w.shortcuts->taken << QKeySequence(Qt::Key_F4);
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *recorder = w.recorder(u"checkKey"_s);
        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F4)));
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), true);
        // it still has the one it had
        QCOMPARE(recorder->property("keySequence").value<QKeySequence>(), QKeySequence(Qt::Key_F2));
        QCOMPARE(w.settings().overlay.hotkey, u"F2"_s);

        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F6)));
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), false);
    }

    void resultsKey()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *recorder = w.recorder(u"resultsKey"_s);
        QCOMPARE(recorder->property("keySequence").value<QKeySequence>(), QKeySequence(u"Ctrl+Shift+D"_s));
        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F7)));
        QCOMPARE(w.shortcuts->keys.value(u"trade-results"_s), QKeySequence(Qt::Key_F7));
        QCOMPARE(w.settings().overlay.resultsHotkey, u"F7"_s);
        // and the other one stays where it was
        QCOMPARE(w.shortcuts->keys.value(u"check-item"_s), QKeySequence(Qt::Key_F2));
        QCOMPARE(w.settings().overlay.hotkey, u"F2"_s);
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), false);
    }

    void resultsKeyTaken()
    {
        // by the check key, as a key can only do the one
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *recorder = w.recorder(u"resultsKey"_s);
        QVERIFY(pressKey(recorder, QKeySequence(Qt::Key_F2)));
        QCOMPARE(w.find(u"refusedText"_s)->property("visible").toBool(), true);
        QCOMPARE(recorder->property("keySequence").value<QKeySequence>(), QKeySequence(u"Ctrl+Shift+D"_s));
        QCOMPARE(w.settings().overlay.resultsHotkey, u"Ctrl+Shift+D"_s);
        QCOMPARE(w.shortcuts->keys.value(u"check-item"_s), QKeySequence(Qt::Key_F2));
    }

    void saysWhenTheKeysOnlyWorkInTheGame()
    {
        Window always;
        QVERIFY(always.show(u"overlayAction"_s));
        QCOMPARE(always.find(u"gameOnlyNote"_s)->property("visible").toBool(), false);

        auto *game = new FakeGameWatcher;
        Window w(true, false, {}, used(), true, game);
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.find(u"gameOnlyNote"_s)->property("visible").toBool(), true);
        QVERIFY(text(w.find(u"gameOnlyNote"_s)).contains(u"only work while Path of Exile 2 is running"_s));
    }

    void noHotkeys()
    {
        Window w(false);
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *error = w.find(u"hotkeysError"_s);
        QCOMPARE(error->property("visible").toBool(), true);
        QCOMPARE(text(error), w.app->overlayOptions()->hotkeysError());
    }

    void popupSettings()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *position = w.find(u"positionBox"_s);
        QCOMPARE(position->property("currentText").toString(), u"Next to the mouse"_s);
        QCOMPARE(w.spinBox(u"autoHide"_s)->property("value").toInt(), 60);
        QVERIFY(activate(position, 2));
        QCOMPARE(w.settings().overlay.position, PopupPosition::Right);

        QVERIFY(setValue(w.find(u"sizeSlider"_s), 1.5, "moved"));
        QCOMPARE(w.settings().overlay.scale, 1.5);
        QCOMPARE(text(w.find(u"sizeText"_s)), u"150%"_s);

        QVERIFY(setValue(w.spinBox(u"autoHide"_s), 30, "valueModified"));
        QCOMPARE(w.settings().overlay.autoHideSeconds, 30);

        QVERIFY(toggle(w.find(u"compactSwitch"_s)));
        QVERIFY(toggle(w.find(u"colorblindSwitch"_s)));
        QVERIFY(toggle(w.find(u"advancedCopySwitch"_s)));
        QVERIFY(toggle(w.find(u"putBackSwitch"_s)));
        QVERIFY(toggle(w.find(u"clipboardSwitch"_s)));
        QVERIFY(toggle(w.find(u"pausedSwitch"_s)));
        const OverlaySettings &overlay = w.settings().overlay;
        QCOMPARE(overlay.compact, false);
        QCOMPARE(overlay.colorblind, true);
        QCOMPARE(overlay.advancedCopy, true);
        QCOMPARE(overlay.putBackClipboard, false);
        QCOMPARE(overlay.watchClipboard, false);
        QCOMPARE(overlay.paused, true);
    }

    void startWithDesktop()
    {
#ifdef Q_OS_WIN
        QSKIP("On Windows it starts from the registry, which the tests keep out of");
#endif
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        // which it is until you turn it off
        QObject *start = w.find(u"startSwitch"_s);
        QCOMPARE(start->property("checked").toBool(), true);
        QVERIFY(QFile::exists(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QVERIFY(!isVisible(w.find(u"startError"_s)));
        QVERIFY(!isVisible(w.find(u"startErrorSeparator"_s)));

        QVERIFY(toggle(start));
        QVERIFY(!w.app->overlayOptions()->startsWithDesktop());
        QVERIFY(!QFile::exists(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QCOMPARE(start->property("checked").toBool(), false);

        QVERIFY(toggle(start));
        QVERIFY(QFile::exists(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QCOMPARE(start->property("checked").toBool(), true);
    }

    void startFailed()
    {
#ifdef Q_OS_WIN
        QSKIP("On Windows it starts from the registry, which the tests keep out of");
#endif
        Window w;
        // off, with a file where the folder should be
        w.app->overlayOptions()->setStartWithDesktop(false);
        QVERIFY(QDir(w.dir.filePath(u"autostart"_s)).removeRecursively());
        QFile blocker(w.dir.filePath(u"autostart"_s));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QVERIFY(w.show(u"overlayAction"_s));

        QObject *start = w.find(u"startSwitch"_s);
        QVERIFY(toggle(start));
        // which then goes back, and says why
        QCOMPARE(start->property("checked").toBool(), false);
        QObject *error = w.find(u"startError"_s);
        QVERIFY(isVisible(error));
        QVERIFY(isVisible(w.find(u"startErrorSeparator"_s)));
        QCOMPARE(text(error), w.app->overlayOptions()->startError());
        QVERIFY(!text(error).isEmpty());

        QVERIFY(QFile::remove(w.dir.filePath(u"autostart"_s)));
        QVERIFY(toggle(start));
        QCOMPARE(start->property("checked").toBool(), true);
        QVERIFY(!isVisible(error));
        QVERIFY(!isVisible(w.find(u"startErrorSeparator"_s)));
    }

    void startRemovedByHand()
    {
#ifdef Q_OS_WIN
        QSKIP("On Windows it starts from the registry, which the tests keep out of");
#endif
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(w.shown());
        QObject *start = w.find(u"startSwitch"_s);
        QCOMPARE(start->property("checked").toBool(), true);

        // while it's behind another window, which it then comes back from
        QWindow other;
        other.show();
        other.requestActivate();
        QTRY_VERIFY(other.isActive());
        QVERIFY(QFile::remove(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QCOMPARE(start->property("checked").toBool(), true);
        w.window->requestActivate();
        QTRY_VERIFY(w.window->isActive());
        QCOMPARE(start->property("checked").toBool(), false);
    }

    void showsPopupSettings()
    {
        Window w;
        Settings settings = w.settings();
        settings.overlay.position = PopupPosition::Top;
        settings.overlay.paused = true;
        QVERIFY(w.app->settings().set(settings));
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.find(u"positionBox"_s)->property("currentText").toString(), u"At the top of the screen"_s);
        QCOMPARE(w.find(u"pausedSwitch"_s)->property("checked").toBool(), true);
        // only as far as the popup goes
        QObject *autoHide = w.spinBox(u"autoHide"_s);
        QCOMPARE(autoHide->property("from").toInt(), 2);
        QCOMPARE(autoHide->property("to").toInt(), 120);
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
