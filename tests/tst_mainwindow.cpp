// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "core/datafiles.h"
#include "fakegameinput.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakepob.h"
#include "tradefixtures.h"

#include <KLocalizedQmlContext>
#include <QDateTime>
#include <QFileInfo>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
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
#include <functional>
#include <memory>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

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
    FakeNetwork *network = online();
    std::unique_ptr<App> app;
    std::unique_ptr<QQuickWindow> window;

    explicit Window(bool hotkeys = true)
    {
        shortcuts->available = hotkeys;
        app = std::make_unique<App>(
            &qml, std::make_unique<FakeGameInput>(), std::unique_ptr<GlobalShortcuts>(shortcuts),
            std::unique_ptr<QNetworkAccessManager>(network),
            PobPlaces{.home = dir.path(), .dataHome = dir.path(), .luajit = QStringLiteral(FAKEPOB)},
            App::Paths{.settings = dir.filePath(u"settings.conf"_s),
                       .data = dir.filePath(u"data"_s),
                       .cache = dir.filePath(u"cache"_s)});
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

    // once poe.ninja has said what currencies are worth
    bool rated() const
    {
        QSignalSpy refreshed(&app->rates(), &RatesStore::refreshed);
        return refreshed.wait();
    }
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
        QVERIFY(w.show(u"charactersAction"_s));
        QCOMPARE(w.page()->property("title").toString(), u"Characters"_s);
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
