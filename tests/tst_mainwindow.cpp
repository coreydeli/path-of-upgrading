// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "core/datafiles.h"
#include "core/parser.h"
#include "core/weights.h"
#include "fakegameinput.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakepob.h"
#include "fakesecretstore.h"
#include "itemtexts.h"
#include "tradefixtures.h"

#include <KLocalizedQmlContext>
#include <QAccessible>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
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
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtQuickTest/quicktest.h>
#include <functional>
#include <memory>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// Made up, and not a real one
const QString fakeId = u"0123456789abcdef0123456789abcdef"_s;

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// The trade site with its data and leagues, then poe.ninja with what
// currencies are worth.
FakeNetwork *online()
{
    QJsonArray leagues;
    for (const QString &id : {u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s, u"Hardcore"_s})
        leagues << QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}};
    auto *network = new FakeNetwork;
    network->answers = {{.body = json(tradefixtures::stats())},
                        {.body = json(tradefixtures::filters())},
                        {.body = json(QJsonObject{{u"result"_s, leagues}})},
                        {.body = R"({"core":{"primary":"divine","rates":{"exalted":400}},"lines":[]})"}};
    return network;
}

// Through the children and the items in them both, as what a Repeater makes
// is only in the items.
QObject *search(QObject *object, const std::function<bool(QObject *)> &wanted, QSet<QObject *> &seen)
{
    if (seen.contains(object))
        return nullptr;
    seen.insert(object);
    if (wanted(object))
        return object;
    QObjectList next = object->children();
    if (const auto *item = qobject_cast<QQuickItem *>(object)) {
        for (QQuickItem *child : item->childItems())
            next << child;
    }
    for (QObject *child : std::as_const(next)) {
        if (QObject *found = search(child, wanted, seen))
            return found;
    }
    return nullptr;
}

QObject *search(QObject *object, const std::function<bool(QObject *)> &wanted)
{
    QSet<QObject *> seen;
    return search(object, wanted, seen);
}

// one of a dialog's buttons, by what it says
QObject *button(QObject *dialog, const QString &text)
{
    return dialog ? search(dialog,
                           [&](QObject *o) {
                               return o->inherits("QQuickAbstractButton") &&
                                      o->property("text").toString() == text;
                           })
                  : nullptr;
}

bool makePob(const QString &folder)
{
    QFile wrapper(folder + u"/HeadlessWrapper.lua"_s);
    return wrapper.open(QIODevice::WriteOnly);
}

Item item(const QString &text)
{
    return parseItem(text).value();
}

const QString jewel = u"Item Class: Jewels\nRarity: Rare\nPale Eye\nRuby\n--------\n+8% to Fire Resistance"_s;
const QString luckyJewel = u"Item Class: Jewels\nRarity: Rare\nGrim Spark\nEmerald\n--------\n"
                           "+8% to Fire Resistance\n10% increased Rarity of Items found"_s;

// a build with nothing on
const QString ranger = u"<PathOfBuilding2><Build level=\"40\" className=\"Ranger\"/></PathOfBuilding2>"_s;

// The app with its window, and stand-ins for the game, the desktop and the
// sites. Path of Building isn't in any of the usual places.
struct Window
{
    QTemporaryDir dir;
    FakePob pob;
    QQmlEngine qml;
    FakeGlobalShortcuts *shortcuts = new FakeGlobalShortcuts;
    FakeSecretStore *keyring = new FakeSecretStore;
    FakeNetwork *network = online();
    std::unique_ptr<App> app;
    std::unique_ptr<QQuickWindow> window;

    explicit Window(bool hotkeys = true)
    {
        shortcuts->available = hotkeys;
        app = std::make_unique<App>(
            &qml, std::make_unique<FakeGameInput>(), std::unique_ptr<GlobalShortcuts>(shortcuts),
            std::unique_ptr<SecretStore>(keyring), std::unique_ptr<QNetworkAccessManager>(network),
            PobPlaces{.home = dir.path(), .dataHome = dir.path(), .luajit = QStringLiteral(FAKEPOB)},
            App::Paths{.settings = dir.filePath(u"settings.conf"_s),
                       .data = dir.filePath(u"data"_s),
                       .cache = dir.filePath(u"cache"_s),
                       .autostart = dir.filePath(u"autostart/made-up.desktop"_s)});
        KLocalization::setupLocalizedContext(&qml);
        QQmlComponent main(&qml, u"PathOfUpgrading"_s, u"Main"_s);
        QObject *made = main.createWithInitialProperties({{u"app"_s, QVariant::fromValue(app.get())}});
        if (!made)
            qWarning() << main.errorString();
        window.reset(qobject_cast<QQuickWindow *>(made));
    }

    const Settings &settings() const { return app->settings().settings(); }

    // as when you pick it in the sidebar
    bool show(const QString &action) const
    {
        QObject *found = window->findChild<QObject *>(action);
        return found && QMetaObject::invokeMethod(found, "trigger");
    }

    QQuickItem *page() const
    {
        const auto *stack = window->property("pageStack").value<QObject *>();
        return stack->property("currentItem").value<QQuickItem *>();
    }

    QObject *find(const QString &name) const
    {
        QObject *found = search(page(), [&](QObject *o) { return o->objectName() == name; });
        if (!found)
            qWarning() << "There's no" << name;
        return found;
    }

    // the spin box in a NumberField
    QObject *spinBox(const QString &field) const
    {
        QObject *row = find(field);
        return row ? row->findChild<QObject *>(u"spinBox"_s) : nullptr;
    }

    QObject *recorder(const QString &field) const
    {
        QObject *row = find(field);
        return row ? row->findChild<QObject *>(u"recorder"_s) : nullptr;
    }

    // what a card has in it by that name
    QObject *in(const QString &card, const QString &name) const
    {
        QObject *found = search(find(card), [&](QObject *o) { return o->objectName() == name; });
        if (!found)
            qWarning() << "There's no" << name << "in" << card;
        return found;
    }

    // a character you play wearing them
    QString play(std::initializer_list<std::pair<QString, Item>> gear, const QSet<QString> &fromPob = {})
    {
        Character character;
        character.name = u"Wren"_s;
        for (const auto &[slot, item] : gear)
            character.gear.insert(slot, item);
        character.fromPob = fromPob;
        const QString id = app->characters().add(character).value_or(QString());
        app->characterList()->load();
        app->characterList()->setCurrent(id);
        return id;
    }

    // once poe.ninja has said what currencies are worth
    bool rated() const
    {
        QSignalSpy refreshed(&app->rates(), &RatesStore::refreshed);
        return refreshed.wait();
    }

    // once the keyring has answered what it was asked
    bool keyringAnswered() const
    {
        return QTest::qWaitFor([this] { return !app->tradeSession()->isBusy(); }, 1000);
    }

    // On the offscreen screen, as a list only makes the rows it shows once
    // it's laid out.
    bool shown() const
    {
        window->resize(1000, 700);
        window->show();
        return QTest::qWaitForWindowExposed(window.get());
    }

    // once what changed is laid out
    bool settled() const { return QQuickTest::qWaitForPolish(window.get()); }
};

// What clicking a switch does.
bool toggle(QObject *control)
{
    return control && control->setProperty("checked", !control->property("checked").toBool()) &&
           QMetaObject::invokeMethod(control, "toggled");
}

// Picking one of a combo box's choices.
bool activate(QObject *box, int index)
{
    return box && box->setProperty("currentIndex", index) &&
           QMetaObject::invokeMethod(box, "activated", Q_ARG(int, index));
}

bool setValue(QObject *control, const QVariant &value, const char *signal)
{
    return control && control->setProperty("value", value) && QMetaObject::invokeMethod(control, signal);
}

bool pressKey(QObject *recorder, const QKeySequence &key)
{
    return recorder && recorder->setProperty("keySequence", key) &&
           QMetaObject::invokeMethod(recorder, "keySequenceModified");
}

// What typing it into a NumberField's spin box comes to, in the box's units.
int typed(QObject *spinBox, const QString &text)
{
    const auto fromText = spinBox->property("valueFromText").value<QJSValue>();
    return fromText.call({text, qjsEngine(spinBox)->toScriptValue(QLocale())}).toInt();
}

QString text(QObject *item)
{
    return item ? item->property("text").toString() : QString();
}

bool click(QObject *button)
{
    return button && QMetaObject::invokeMethod(button, "clicked");
}

// A key pressed in a field, as though it had the focus.
bool press(QObject *field, Qt::Key key)
{
    if (!field)
        return false;
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(field, &event);
    return true;
}

bool isVisible(QObject *item)
{
    return item && item->property("visible").toBool();
}

} // namespace

class TestMainWindow : public QObject
{
    Q_OBJECT

public:
    // The window isn't for the screen. Kirigami would ask the desktop portal
    // for the look of the desktop, which starts it.
    static void initMain()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    }

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void opens()
    {
        Window w;
        QVERIFY(w.window);
        QCOMPARE(w.window->title(), u"Path of Upgrading"_s);
        QCOMPARE(w.page()->property("title").toString(), u"Characters"_s);
    }

    void noCharacters()
    {
        Window w;
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
        QObject *name = w.find(u"nameField"_s);
        QVERIFY(name->setProperty("text", u"  "_s));
        QVERIFY(QMetaObject::invokeMethod(name, "accepted"));
        QCOMPARE(w.app->characterList()->rowCount(), 0);
        QObject *error = w.find(u"charactersError"_s);
        QVERIFY(isVisible(error));
        QCOMPARE(text(error), u"Give the character a name."_s);
    }

    void pickCharacter()
    {
        Window w;
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

    void renameCharacter()
    {
        Window w;
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
        QObject *code = w.find(u"codeField"_s);
        QVERIFY(code->setProperty("text", ranger));
        QVERIFY(click(w.find(u"importButton"_s)));
        QObject *message = w.find(u"importMessage"_s);
        QVERIFY(isVisible(message));
        QCOMPARE(text(message), u"Imported the level 40 Ranger build for Ranger. It has no items on."_s);
        QCOMPARE(text(code), QString());

        // on a new one, as there was none to play
        QObject *row = w.find(u"character:Ranger"_s);
        QVERIFY(row->property("checked").toBool());
        QCOMPARE(row->property("description").toString(), u"Level 40 Ranger"_s);
    }

    void importFails()
    {
        Window w;
        QObject *code = w.find(u"codeField"_s);
        QVERIFY(code->setProperty("text", u"nonsense"_s));
        QVERIFY(QMetaObject::invokeMethod(code, "accepted"));
        QCOMPARE(text(w.find(u"importMessage"_s)), u"That isn't a Path of Building code or a link to one."_s);
        // to put right
        QCOMPARE(text(code), u"nonsense"_s);
        QCOMPARE(w.app->characterList()->rowCount(), 0);
    }

    void downloading()
    {
        Window w;
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
        const QString folder = w.pob.folder.path();
        QVERIFY(makePob(folder));
        w.app->pobOptions()->setLocation(folder);
        QVERIFY(isVisible(w.find(u"savedBuilds"_s)));
        const QString builds = w.app->buildImporter()->buildsFolder();
        QCOMPARE(text(w.find(u"noSavedBuildsText"_s)), u"There are none in %1 yet."_s.arg(builds));

        QVERIFY(writeFile(builds + u"/Leveling.xml"_s, ranger.toUtf8()));
        QVERIFY(click(w.find(u"refreshBuildsButton"_s)));
        QVERIFY(!isVisible(w.find(u"noSavedBuildsText"_s)));
        QObject *saved = w.find(u"saved:Leveling"_s);
        const QDateTime modified = QFileInfo(builds + u"/Leveling.xml"_s).lastModified();
        QCOMPARE(saved->property("description").toString(),
                 u"Saved %1"_s.arg(QLocale().toString(modified, QLocale::ShortFormat)));
        QVERIFY(click(saved));
        QCOMPARE(text(w.find(u"importMessage"_s)),
                 u"Imported the level 40 Ranger build for Leveling. It has no items on."_s);
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
        QObject *found = w.find(u"foundText"_s);
        QCOMPARE(text(found), u"Path of Building wasn't found."_s);
        QVERIFY(found->property("description").toString().startsWith(u"Path of Building isn't installed."_s));
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
        QCOMPARE(text(w.find(u"foundText"_s)), u"Using Path of Building in %1."_s.arg(folder));
        QCOMPARE(w.find(u"foundText"_s)->property("description").toString(), QString());
    }

    void lookAgain()
    {
        Window w;
        QVERIFY(w.show(u"pobAction"_s));
        const QString folder = w.pob.folder.path();
        w.app->pobOptions()->setLocation(folder);
        QCOMPARE(text(w.find(u"foundText"_s)), u"Path of Building wasn't found."_s);

        // once it's there
        QVERIFY(makePob(folder));
        QVERIFY(QMetaObject::invokeMethod(w.find(u"lookAgainButton"_s), "clicked"));
        QCOMPARE(text(w.find(u"foundText"_s)), u"Using Path of Building in %1."_s.arg(folder));
    }

    void pobSettings()
    {
        Window w;
        QVERIFY(w.show(u"pobAction"_s));
        QVERIFY(toggle(w.find(u"chaosSwitch"_s)));
        QVERIFY(toggle(w.find(u"keepDamageSwitch"_s)));
        QVERIFY(toggle(w.find(u"spiritSwitch"_s)));
        QVERIFY(toggle(w.find(u"attributesSwitch"_s)));
        const PobPriorities &priorities = w.settings().pob.priorities;
        QCOMPARE(priorities.chaos, true);
        QCOMPARE(priorities.keepDamage, false);
        QCOMPARE(priorities.spirit, false);
        QCOMPARE(priorities.attributes, false);

        QVERIFY(toggle(w.find(u"enabledSwitch"_s)));
        QCOMPARE(w.settings().pob.enabled, false);
        // which it needs
        QCOMPARE(w.find(u"keepDamageSwitch"_s)->property("enabled").toBool(), false);

        QVERIFY(toggle(w.find(u"resistsSwitch"_s)));
        QCOMPARE(w.settings().pob.priorities.resistsFirst, false);
        QVERIFY(setValue(w.find(u"damageSlider"_s), 0.3, "moved"));
        QCOMPARE(w.settings().pob.damageWeight, 0.3);
    }

    // as you left them last time
    void showsPobSettings()
    {
        Window w;
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

    void pages()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Trade"_s);
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Popup and Hotkeys"_s);
        QVERIFY(w.show(u"pobAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Path of Building"_s);
        QVERIFY(w.show(u"gearAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Your Gear"_s);
        QVERIFY(w.show(u"gearScoreAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Gear Score"_s);
        QVERIFY(w.show(u"weightsAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Stat Weights"_s);
        QVERIFY(w.show(u"ignoredAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Ignored Stats"_s);
        QVERIFY(w.show(u"checkedAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Checked Items"_s);
        QVERIFY(w.show(u"charactersAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Characters"_s);
    }

    void gearForNobody()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        QVERIFY(isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(!isVisible(w.find(u"gearHelp"_s)));
        QVERIFY(!isVisible(w.find(u"gearError"_s)));
        QVERIFY(!isVisible(w.find(u"extras"_s)));
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
        QCOMPARE(helmet->property("description").toString(), u"Grim Crown, Visored Helm"_s);
        QObject *description = helmet->findChild<QObject *>(u"descriptionText"_s);
        QVERIFY(isVisible(description));
        QCOMPARE(text(description), u"Grim Crown, Visored Helm"_s);
        QObject *score = search(helmet, [](QObject *o) { return o->objectName() == u"gearScore"; });
        QVERIFY(isVisible(score));
        const QString number = w.app->gearList()->index(2).data(GearList::ScoreRole).toString();
        const QString grade = w.app->gearList()->index(2).data(GearList::GradeRole).toString();
        QVERIFY(!number.isEmpty());
        QCOMPARE(text(score), u"Gear score %1 %2"_s.arg(number, grade));
        QVERIFY(isVisible(helmet->findChild<QObject *>(u"takeOffButton"_s)));

        QCOMPARE(w.find(u"slot:Ring 1"_s)->property("description").toString(),
                 u"Storm Loop, Sapphire Ring, from Path of Building"_s);

        QObject *gloves = w.find(u"slot:Gloves"_s);
        QCOMPARE(gloves->property("description").toString(), u"Nothing saved"_s);
        QVERIFY(!isVisible(search(gloves, [](QObject *o) { return o->objectName() == u"gearScore"; })));
        QVERIFY(!isVisible(gloves->findChild<QObject *>(u"takeOffButton"_s)));
    }

    void gearHeading()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        QObject *heading = search(w.page(), [](QObject *o) {
            return o->inherits("QQuickItem") && o->property("title").toString().startsWith(u"Worn"_s);
        });
        QCOMPARE(heading->property("title").toString(), u"Worn"_s);
        w.play({});
        QCOMPARE(heading->property("title").toString(), u"Worn by Wren"_s);
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

    void takeOffGear()
    {
        Window w;
        QVERIFY(w.show(u"gearAction"_s));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        QVERIFY(click(w.find(u"slot:Helmet"_s)->findChild<QObject *>(u"takeOffButton"_s)));
        QVERIFY(w.app->character().character().gear.isEmpty());
        QCOMPARE(w.find(u"slot:Helmet"_s)->property("description").toString(), u"Nothing saved"_s);
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

    void gearScoreForNobody()
    {
        Window w;
        QVERIFY(w.show(u"gearScoreAction"_s));
        QVERIFY(isVisible(w.find(u"nobodyText"_s)));
        QVERIFY(!isVisible(w.find(u"profileBox"_s)));
        QVERIFY(!isVisible(w.find(u"scoreHelp"_s)));
        QVERIFY(!isVisible(w.find(u"stats"_s)));
        QVERIFY(!isVisible(w.find(u"scoreError"_s)));
    }

    void gearScore()
    {
        Window w;
        QVERIFY(w.show(u"gearScoreAction"_s));
        w.play({});
        QVERIFY(!isVisible(w.find(u"nobodyText"_s)));
        QObject *heading = search(w.page(), [](QObject *o) {
            return o->inherits("QQuickItem") && o->property("title").toString() == u"For Wren";
        });
        QVERIFY(isVisible(heading));

        QObject *profile = w.find(u"profileBox"_s);
        QVERIFY(isVisible(profile));
        QCOMPARE(profile->property("currentText").toString(), u"Life and resistances"_s);
        QCOMPARE(profile->property("description").toString(), w.app->gearScoring()->summary());
        QVERIFY(isVisible(w.find(u"scoreHelp"_s)));
        QVERIFY(isVisible(w.find(u"stats"_s)));

        QObject *life = w.in(u"stats"_s, u"stat:Life"_s);
        QCOMPARE(text(life), u"Life"_s);
        QCOMPARE(life->property("description").toString(), u"Counts in every slot but Weapon and Jewel"_s);
        QObject *level = life->findChild<QObject *>(u"levelBox"_s);
        QVERIFY(isVisible(level));
        QCOMPARE(level->property("currentText").toString(), u"As usual"_s);
        QCOMPARE(w.find(u"resetButton"_s)->property("enabled").toBool(), false);

        // and one picked elsewhere
        w.app->gearScoring()->setProfile(2);
        QCOMPARE(profile->property("currentText").toString(), u"Minion summoner on Energy Shield"_s);
    }

    void statLevels()
    {
        Window w;
        QVERIFY(w.show(u"gearScoreAction"_s));
        w.play({});
        QObject *life = w.in(u"stats"_s, u"stat:Life"_s)->findChild<QObject *>(u"levelBox"_s);
        QVERIFY(activate(life, 4));
        QCOMPARE(w.app->character().character().tweaks, ProfileTweaks({{u"Life"_s, 2.0}}));
        QCOMPARE(life->property("currentText").toString(), u"Twice as much"_s);

        QObject *reset = w.find(u"resetButton"_s);
        QCOMPARE(reset->property("enabled").toBool(), true);
        QVERIFY(click(reset));
        QCOMPARE(w.app->character().character().tweaks, ProfileTweaks());
        QCOMPARE(life->property("currentText").toString(), u"As usual"_s);
        QCOMPARE(reset->property("enabled").toBool(), false);
    }

    void pickBuildProfile()
    {
        Window w;
        QVERIFY(w.show(u"gearScoreAction"_s));
        w.play({});
        QObject *profile = w.find(u"profileBox"_s);
        QVERIFY(activate(profile, 1));
        QCOMPARE(w.app->character().character().buildProfile, u"es-caster"_s);
        QVERIFY(isVisible(w.in(u"stats"_s, u"stat:Spell skill levels"_s)));

        // and none, by your stat weights alone
        QVERIFY(activate(profile, 3));
        QCOMPARE(w.app->character().character().buildProfile, u"none"_s);
        QCOMPARE(profile->property("description").toString(),
                 u"Items are judged by your stat weights alone, with no gear score."_s);
        QVERIFY(!isVisible(w.find(u"stats"_s)));
        QVERIFY(!isVisible(w.find(u"scoreHelp"_s)));
    }

    void cantSaveGearScore()
    {
        Window w;
        QVERIFY(w.show(u"gearScoreAction"_s));
        const QString id = w.play({});
        const QString file = w.dir.filePath(u"data/characters/"_s + id + u".json"_s);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkdir(file));
        QVERIFY(activate(w.find(u"profileBox"_s), 1));
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
        QVERIFY(!isVisible(w.find(u"addStatButton"_s)));
        QVERIFY(!isVisible(w.find(u"suggestButton"_s)));
        QVERIFY(!isVisible(w.find(u"resetButton"_s)));
        QVERIFY(!isVisible(w.find(u"weightsHelp"_s)));
        QVERIFY(!isVisible(w.find(u"weightsError"_s)));
        QVERIFY(!isVisible(w.find(u"undoMessage"_s)));
    }

    void weights()
    {
        Window w;
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
        QCOMPARE(armour->property("description").toString(), u"The item's own, with its mods"_s);
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

    void pickerWaitsForTheSite()
    {
        Window w;
        QVERIFY(w.show(u"weightsAction"_s));
        w.play({});
        QVERIFY(click(w.find(u"addStatButton"_s)));
        QObject *message = w.find(u"pickerMessage"_s);
        QVERIFY(isVisible(message));
        QCOMPARE(text(message), u"Getting the list of stats from the trade site."_s);
        QCOMPARE(w.app->statPicker()->rowCount(), equipmentKeys().size());
        QTRY_VERIFY(!isVisible(message));
        QCOMPARE(w.app->statPicker()->rowCount(), equipmentKeys().size() + 4);
    }

    void nothingToSuggest()
    {
        Window w;
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
        QVERIFY(!isVisible(w.find(u"ignoredHelp"_s)));
        QVERIFY(!isVisible(w.find(u"singleStats"_s)));
        QVERIFY(!isVisible(w.find(u"ignoredError"_s)));
        QVERIFY(!search(w.page(), [](QObject *o) { return o->objectName().startsWith(u"group:"); }));
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
        QCOMPARE(text(fire->findChild<QObject *>(u"statNote"_s)), u"You ignore it already"_s);

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
        QCOMPARE(none->property("description").toString(), u"Hover over an item in the game and press F2."_s);
        QCOMPARE(w.find(u"checkedHeader"_s)->property("title").toString(), u"Newest First"_s);
        QVERIFY(!isVisible(w.find(u"forgetSeparator"_s)));
        QVERIFY(!isVisible(w.find(u"forgetButton"_s)));
        QVERIFY(!isVisible(w.find(u"checkedHelp"_s)));
        QVERIFY(!search(w.page(), [](QObject *o) { return o->objectName().startsWith(u"checked:"); }));

        // with the key it has now
        QVERIFY(w.app->overlayOptions()->setCheckKey(QKeySequence(Qt::Key_F6)));
        QCOMPARE(none->property("description").toString(), u"Hover over an item in the game and press F6."_s);
    }

    void nothingCheckedWithoutHotkeys()
    {
        Window w(false);
        QVERIFY(w.show(u"checkedAction"_s));
        QCOMPARE(w.find(u"noItemsText"_s)->property("description").toString(),
                 u"Hover over an item in the game and press the hotkey for checking one. You can set it on "
                 "the Popup and Hotkeys page."_s);
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
        // as the switch says so
        QCOMPARE(w.find(u"unavailableText"_s)->property("visible").toBool(), false);
        QVERIFY(!w.app->checker().popup().isTradeAvailable());
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
        QCOMPARE(autoRate->property("description").toString(),
                 u"A Divine Orb is 400 Exalted Orbs in Runes of Aldur."_s);
        // which goes before yours
        QCOMPARE(w.find(u"divineRate"_s)->property("enabled").toBool(), false);
        QVERIFY(toggle(autoRate));
        QCOMPARE(w.settings().trade.autoRate, false);
        QCOMPARE(w.find(u"divineRate"_s)->property("enabled").toBool(), true);
        QVERIFY(setValue(w.spinBox(u"divineRate"_s), 3505, "valueModified"));
        QCOMPARE(w.settings().trade.budget.divineRate, 350.5);
    }

    void noRateYet()
    {
        Window w;
        QVERIFY(w.show(u"tradeAction"_s));
        QCOMPARE(w.find(u"autoRateSwitch"_s)->property("description").toString(),
                 u"It hasn't said yet, so it goes by yours."_s);
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
        QCOMPARE(text(error), u"That isn't a session ID. It's only letters and digits."_s);
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
        QVERIFY(toggle(w.find(u"clipboardSwitch"_s)));
        QVERIFY(toggle(w.find(u"pausedSwitch"_s)));
        const OverlaySettings &overlay = w.settings().overlay;
        QCOMPARE(overlay.compact, false);
        QCOMPARE(overlay.colorblind, true);
        QCOMPARE(overlay.advancedCopy, true);
        QCOMPARE(overlay.watchClipboard, false);
        QCOMPARE(overlay.paused, true);
    }

    void startWithDesktop()
    {
        Window w;
        QVERIFY(w.show(u"overlayAction"_s));
        QObject *start = w.find(u"startSwitch"_s);
        QCOMPARE(start->property("checked").toBool(), false);
        QVERIFY(!isVisible(w.find(u"startError"_s)));
        QVERIFY(!isVisible(w.find(u"startErrorSeparator"_s)));

        QVERIFY(toggle(start));
        QVERIFY(w.app->overlayOptions()->startsWithDesktop());
        QVERIFY(QFile::exists(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QCOMPARE(start->property("checked").toBool(), true);

        QVERIFY(toggle(start));
        QVERIFY(!QFile::exists(w.dir.filePath(u"autostart/made-up.desktop"_s)));
        QCOMPARE(start->property("checked").toBool(), false);
    }

    void startsWithDesktopAlready()
    {
        Window w;
        w.app->overlayOptions()->setStartWithDesktop(true);
        QVERIFY(w.show(u"overlayAction"_s));
        QCOMPARE(w.find(u"startSwitch"_s)->property("checked").toBool(), true);
    }

    void startFailed()
    {
        Window w;
        // a file where the folder should be
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
        Window w;
        w.app->overlayOptions()->setStartWithDesktop(true);
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
