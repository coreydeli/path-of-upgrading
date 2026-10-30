// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "app/commandline.h"
#include "core/buildprofiles.h"
#include "core/parser.h"
#include "core/rates.h"
#include "core/tradesite.h"
#include "fakegameinput.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakepob.h"
#include "fakesecretstore.h"
#include "itemtexts.h"
#include "tradefixtures.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

const QString build = u"<PathOfBuilding2><Build level=\"90\" className=\"Warrior\"/></PathOfBuilding2>"_s;

Character wearing(const QString &name, const QString &helmet)
{
    Character c;
    c.name = name;
    c.gear = {{u"Helmet"_s, parseItem(helmet).value()}};
    return c;
}

// The app's folders before it starts, with Path of Building in one of its own.
struct Folders
{
    QTemporaryDir dir;
    App::Paths paths{.settings = dir.filePath(u"settings.conf"_s),
                     .data = dir.filePath(u"data"_s),
                     .cache = dir.filePath(u"cache"_s),
                     .autostart = dir.filePath(u"autostart/made-up.desktop"_s)};
    CharacterStore characters{dir.filePath(u"data/characters"_s)};
    FakePob pob;

    Folders()
    {
        QFile wrapper(pob.folder.filePath(u"HeadlessWrapper.lua"_s));
        if (wrapper.open(QIODevice::WriteOnly))
            wrapper.close();
    }

    // where it's run with the fake for LuaJIT
    PobPlaces places() const
    {
        return {.home = dir.path(), .dataHome = dir.path(), .luajit = QStringLiteral(FAKEPOB)};
    }

    QString add(const Character &character, const QString &xml = {})
    {
        const QString id = characters.add(character).value_or(QString());
        if (!xml.isEmpty())
            characters.savePobBuild(id, xml);
        return id;
    }

    void set(const Settings &settings) const { SettingsStore(paths.settings).set(settings); }
};

// What the desktop has for the app's shortcuts before it starts.
FakeGlobalShortcuts *desktop(const QHash<QString, QKeySequence> &keys = {}, bool available = true)
{
    auto *shortcuts = new FakeGlobalShortcuts;
    shortcuts->keys = keys;
    shortcuts->available = available;
    return shortcuts;
}

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// The trade site and poe.ninja, with what they say as the app starts: the
// site's data with its leagues, then what currencies are worth in one.
// What the trade site and then poe.ninja say when the app starts.
QList<FakeAnswer> siteAnswers(const QJsonObject &stats = tradefixtures::stats())
{
    QJsonArray leagues;
    for (const QString &id : {u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s, u"Hardcore"_s})
        leagues << QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}};
    return {{.body = json(stats)},
            {.body = json(tradefixtures::filters())},
            {.body = json(QJsonObject{{u"result"_s, leagues}})},
            {.body = R"({"core":{"primary":"divine","rates":{"exalted":400}},"lines":[]})"}};
}

FakeNetwork *online(const QJsonObject &stats = tradefixtures::stats())
{
    auto *network = new FakeNetwork;
    network->answers = siteAnswers(stats);
    return network;
}

// Made up, and not a real one
const QString fakeId = u"0123456789abcdef0123456789abcdef"_s;
const QString otherId = u"fedcba9876543210fedcba9876543210"_s;

// The keyring before the app starts, with a session id in it or without.
FakeSecretStore *keyringWith(const QString &id = {})
{
    auto *keyring = new FakeSecretStore;
    if (!id.isEmpty())
        keyring->secrets.insert(u"POESESSID"_s, id);
    return keyring;
}

// Two listings, as the site's fetch answers have them.
FakeAnswer twoListings()
{
    QJsonArray result;
    for (const QString &id : {u"a"_s, u"b"_s}) {
        const QJsonObject account{{u"name"_s, u"Seller%1#1234"_s.arg(id)}};
        const QJsonObject price{{u"type"_s, u"~price"_s}, {u"amount"_s, 3}, {u"currency"_s, u"divine"_s}};
        const QJsonObject item{{u"rarity"_s, u"Rare"_s},
                               {u"name"_s, u"Grim Ward"_s},
                               {u"typeLine"_s, u"Iron Hat"_s},
                               {u"explicitMods"_s, QJsonArray{u"+80 to maximum Life"_s}}};
        result << QJsonObject{{u"id"_s, id},
                              {u"listing"_s, QJsonObject{{u"account"_s, account}, {u"price"_s, price}}},
                              {u"item"_s, item}};
    }
    return {.body = json({{u"result"_s, result}})};
}

// The app with stand-ins for the game, the desktop, the keyring and the sites
// it asks.
struct Running
{
    QQmlEngine qml;
    FakeGameInput *input = new FakeGameInput;
    FakeGlobalShortcuts *shortcuts;
    FakeSecretStore *keyring;
    FakeNetwork *network;
    App app;

    explicit Running(const Folders &folders, FakeGlobalShortcuts *theirs = desktop(),
                     FakeNetwork *sites = online(), FakeSecretStore *theirKeyring = keyringWith())
        : shortcuts(theirs)
        , keyring(theirKeyring)
        , network(sites)
        , app(&qml, std::unique_ptr<GameInput>(input), std::unique_ptr<GlobalShortcuts>(shortcuts),
              std::unique_ptr<SecretStore>(keyring), std::unique_ptr<QNetworkAccessManager>(network),
              folders.places(), folders.paths)
    {}

    // once the keyring has said, which it does a moment after the app starts
    bool signedIn()
    {
        return QTest::qWaitFor([this] { return !app.tradeSession()->isBusy(); }, 1000);
    }

    // once poe.ninja has said what currencies are worth
    bool rated()
    {
        QSignalSpy refreshed(&app.rates(), &RatesStore::refreshed);
        return refreshed.wait();
    }

    QList<QUrl> asked() const
    {
        QList<QUrl> urls;
        for (const FakeNetwork::Sent &sent : network->sent)
            urls << sent.request.url();
        return urls;
    }

    // what the popup says of the helmet, once you've checked it
    ItemReport checked()
    {
        app.checker().check();
        Q_EMIT input->itemCopied(itemtexts::helmetNew, std::nullopt);
        return app.checker().popup().report();
    }
};

} // namespace

class TestApp : public QObject
{
    Q_OBJECT

public:
    // the popup's windows aren't for the screen
    static void initMain() { qputenv("QT_QPA_PLATFORM", "offscreen"); }

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void yourCharacter()
    {
        Folders folders;
        Settings settings;
        settings.character = folders.add(wearing(u"Torvald"_s, itemtexts::helmetOld), build);
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.app.character().character().name, u"Torvald"_s);
        QVERIFY(r.app.character().build());
        QCOMPARE(r.checked().versus, u"vs Grim Crown"_s);
    }

    void anotherCharacter()
    {
        Folders folders;
        Settings settings;
        settings.character = folders.add(wearing(u"Torvald"_s, itemtexts::helmetNew));
        const QString other = folders.add(wearing(u"Astrid"_s, itemtexts::helmetOld));
        folders.set(settings);
        Running r(folders);
        settings.character = other;
        QVERIFY(r.app.settings().set(settings));
        QCOMPARE(r.app.character().character().name, u"Astrid"_s);
        QCOMPARE(r.checked().versus, u"vs Grim Crown"_s);
    }

    void charactersPage()
    {
        Folders folders;
        Settings settings;
        settings.character = folders.add(wearing(u"Torvald"_s, itemtexts::helmetOld));
        const QString astrid = folders.add(wearing(u"Astrid"_s, itemtexts::helmetNew));
        settings.pob.location = folders.pob.folder.path();
        folders.set(settings);
        Running r(folders);
        CharacterList *list = r.app.characterList();
        QCOMPARE(list->rowCount(), 2);
        QCOMPARE(list->current(), settings.character);

        // which you then play
        list->setCurrent(astrid);
        QCOMPARE(r.app.character().character().name, u"Astrid"_s);
        QVERIFY(list->remove(astrid));
        QCOMPARE(r.app.character().character().name, u"Torvald"_s);

        // and its build, which items are then judged with
        QVERIFY(!r.app.character().build());
        r.app.buildImporter()->importText(build);
        QVERIFY(!r.app.buildImporter()->hasFailed());
        QVERIFY(r.app.character().build());
        QVERIFY(r.checked().pobChecking);
        QCOMPARE(r.app.buildImporter()->buildsFolder(),
                 folders.pob.folder.path() + u"/userdata/Path of Building (PoE2)/Builds"_s);
    }

    void yourSettings()
    {
        Folders folders;
        Settings settings;
        settings.overlay.scale = 1.5;
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.app.checker().popup().scale(), 1.5);

        settings.overlay.scale = 0.75;
        QVERIFY(r.app.settings().set(settings));
        QCOMPARE(r.app.checker().popup().scale(), 0.75);
    }

    void pobFound()
    {
        Folders folders;
        Settings settings;
        settings.pob.location = folders.pob.folder.path();
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.app.pob().state(), PobEngine::State::Stopped);
    }

    void noPob()
    {
        Folders folders;
        Running r(folders);
        QCOMPARE(r.app.pob().state(), PobEngine::State::Unavailable);
        QVERIFY(r.app.pob().errorString().startsWith(u"Path of Building isn't installed."_s));
    }

    void pobMoved()
    {
        Folders folders;
        Running r(folders);
        Settings settings;
        settings.pob.location = folders.pob.folder.path();
        QVERIFY(r.app.settings().set(settings));
        QCOMPARE(r.app.pob().state(), PobEngine::State::Stopped);
    }

    void lookAgain()
    {
        // once you've put it where you said it was
        Folders folders;
        QTemporaryDir later;
        Settings settings;
        settings.pob.location = later.path();
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.app.pob().state(), PobEngine::State::Unavailable);
        QFile wrapper(later.filePath(u"HeadlessWrapper.lua"_s));
        QVERIFY(wrapper.open(QIODevice::WriteOnly));
        wrapper.close();
        r.app.lookForPob();
        QCOMPARE(r.app.pob().state(), PobEngine::State::Stopped);
    }

    void settingsPages()
    {
        // which change what it runs with
        Folders folders;
        Running r(folders);
        r.app.pobOptions()->setLocation(folders.pob.folder.path());
        QCOMPARE(r.app.pob().state(), PobEngine::State::Stopped);
        QCOMPARE(r.app.pobOptions()->found(),
                 u"Using Path of Building in %1."_s.arg(folders.pob.folder.path()));

        QVERIFY(r.app.overlayOptions()->setCheckKey(QKeySequence(u"F6"_s)));
        QCOMPARE(r.shortcuts->keys.value(u"check-item"_s), QKeySequence(u"F6"_s));
        QCOMPARE(r.app.settings().settings().overlay.hotkey, u"F6"_s);

        r.app.tradeOptions()->setSoloSelfFound(true);
        QVERIFY(!r.app.checker().popup().isTradeAvailable());
        QCOMPARE(r.app.tradeOptions()->unavailable(), r.app.trade().unavailable());
    }

    void checkKey()
    {
        // the one in the settings, the first time
        Folders folders;
        Settings settings;
        settings.overlay.hotkey = u"F3"_s;
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.shortcuts->keys.value(u"check-item"_s), QKeySequence(u"F3"_s));
        // which isn't for the trade results
        Q_EMIT r.shortcuts->activated(u"trade-results"_s);
        QCOMPARE(r.input->copies, 0);
        Q_EMIT r.shortcuts->activated(u"check-item"_s);
        QCOMPARE(r.input->copies, 1);
    }

    void resultsKey()
    {
        Folders folders;
        Running r(folders);
        ResultsPanel &panel = r.app.resultsPanel();
        QVERIFY(!panel.isVisible());
        // which the check key doesn't
        Q_EMIT r.shortcuts->activated(u"check-item"_s);
        QVERIFY(!panel.isVisible());
        Q_EMIT r.shortcuts->activated(u"trade-results"_s);
        QVERIFY(panel.isVisible());
        Q_EMIT r.shortcuts->activated(u"trade-results"_s);
        QVERIFY(!panel.isVisible());
    }

    void resultsKeyWhilePaused()
    {
        Folders folders;
        Settings settings;
        settings.overlay.paused = true;
        folders.set(settings);
        Running r(folders);
        Q_EMIT r.shortcuts->activated(u"trade-results"_s);
        QVERIFY(!r.app.resultsPanel().isVisible());

        settings.overlay.paused = false;
        QVERIFY(r.app.settings().set(settings));
        Q_EMIT r.shortcuts->activated(u"trade-results"_s);
        QVERIFY(r.app.resultsPanel().isVisible());
    }

    void resultsWindowFollowsTheSettings()
    {
        // as they are when the app starts, and as they change
        Folders folders;
        Settings settings;
        settings.overlay.scale = 1.25;
        settings.overlay.colorblind = true;
        folders.set(settings);
        Running r(folders);
        ResultsPanel &panel = r.app.resultsPanel();
        QCOMPARE(panel.scale(), 1.25);
        QVERIFY(panel.isColorblind());
        QVERIFY(panel.unavailable().isEmpty());

        settings.overlay.scale = 1.5;
        settings.overlay.colorblind = false;
        settings.trade.enabled = false;
        QVERIFY(r.app.settings().set(settings));
        QCOMPARE(panel.scale(), 1.5);
        QVERIFY(!panel.isColorblind());
        QCOMPARE(panel.unavailable(), u"Trade searches are turned off in the settings."_s);

        settings.trade.enabled = true;
        QVERIFY(r.app.settings().set(settings));
        QVERIFY(panel.unavailable().isEmpty());
    }

    void resultsFromThePopup()
    {
        // on the screen the item was read on, whatever the mouse does since
        Folders folders;
        Character character;
        character.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(character);
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.rated());
        r.app.checker().check();
        Q_EMIT r.input->itemCopied(itemtexts::helmetNew,
                                   ScreenPoint{.screen = u"DP-1"_s, .position = {20, 20}});
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}, twoListings()};
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QVERIFY(r.app.checker().popup().trade().listed);
        QVERIFY(!r.app.resultsPanel().isVisible());

        r.input->mouseAt = ScreenPoint{.screen = u"DP-9"_s, .position = {5, 5}};
        r.app.checker().popup().showResults();
        QVERIFY(r.app.resultsPanel().isVisible());
        QCOMPARE(r.app.resultsPanel().screenName(), u"DP-1"_s);
        QTRY_COMPARE(r.app.results().rowCount(), 2);
    }

    void resultsKeyShowsTheLastSearch()
    {
        // the listings are asked for as the window opens
        Folders folders;
        Character character;
        character.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(character);
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.rated());
        r.checked();
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}, twoListings()};
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QCOMPARE(r.app.results().rowCount(), 0);

        Q_EMIT r.shortcuts->activated(u"trade-results"_s);
        QTRY_COMPARE(r.app.results().rowCount(), 2);
        QVERIFY(r.app.resultsPanel().isVisible());
    }

    void desktopsKeys()
    {
        // changed in System Settings while the app wasn't running
        Folders folders;
        Running r(folders, desktop({{u"check-item"_s, QKeySequence(u"F5"_s)}}));
        QCOMPARE(r.app.settings().settings().overlay.hotkey, u"F5"_s);
        QCOMPARE(SettingsStore(folders.paths.settings).settings().overlay.hotkey, u"F5"_s);
        // for what you're told to press
        r.app.checker().check();
        Q_EMIT r.input->copyFailed(GameInput::Failure::NothingCopied, {}, std::nullopt);
        QVERIFY(r.app.checker().popup().message().contains(u"press F5"_s));
    }

    void changedOnTheDesktop()
    {
        Folders folders;
        Running r(folders);
        r.shortcuts->change(u"trade-results"_s, QKeySequence(u"Ctrl+T"_s));
        QCOMPARE(r.app.settings().settings().overlay.resultsHotkey, u"Ctrl+T"_s);
    }

    void startsWithTheDesktop()
    {
        const QString name = QGuiApplication::applicationDisplayName();
        const QString file = QGuiApplication::desktopFileName();
        QGuiApplication::setApplicationDisplayName(u"Made-up Program"_s);
        QGuiApplication::setDesktopFileName(u"io.example.MadeUp"_s);
        Folders folders;
        Running r(folders);
        QVERIFY(!r.app.overlayOptions()->startsWithDesktop());
        r.app.overlayOptions()->setStartWithDesktop(true);
        QVERIFY(r.app.overlayOptions()->startsWithDesktop());

        QFile entry(folders.paths.autostart);
        QVERIFY(entry.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(entry.readAll());
        // which starts it the way the command line understands
        QVERIFY(text.contains(QString(u"\nExec="_s + CommandLine::backgroundCommand() + u"\n"_s)));
        QVERIFY(text.contains(u"\nName=Made-up Program\n"_s));
        QVERIFY(text.contains(u"\nIcon=io.example.MadeUp\n"_s));
        QGuiApplication::setApplicationDisplayName(name);
        QGuiApplication::setDesktopFileName(file);
    }

    void withoutAFileToStartFrom()
    {
        Folders folders;
        folders.paths.autostart.clear();
        Running r(folders);
        r.app.overlayOptions()->setStartWithDesktop(true);
        QVERIFY(!r.app.overlayOptions()->startsWithDesktop());
        QVERIFY(!r.app.overlayOptions()->startError().isEmpty());
    }

    void foldersOnTheDesktop()
    {
        QTemporaryDir config;
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        const QString file = QGuiApplication::desktopFileName();
        QGuiApplication::setDesktopFileName(u"io.example.MadeUp"_s);
        QCOMPARE(App::defaultPaths().autostart, config.filePath(u"autostart/io.example.MadeUp.desktop"_s));
        QGuiApplication::setDesktopFileName(file);
        qunsetenv("XDG_CONFIG_HOME");
    }

    void yourKey()
    {
        Folders folders;
        Running r(folders);
        QVERIFY(r.app.hotkeys().setKey(Hotkeys::Hotkey::Check, u"F6"_s));
        QCOMPARE(r.app.settings().settings().overlay.hotkey, u"F6"_s);
    }

    void noShortcuts()
    {
        // where the desktop has none, the keys you chose are kept
        Folders folders;
        Settings settings;
        settings.overlay.hotkey = u"F7"_s;
        folders.set(settings);
        Running r(folders, desktop({}, false));
        QCOMPARE(r.app.settings().settings().overlay.hotkey, u"F7"_s);
    }

    void history()
    {
        Folders folders;
        {
            Running r(folders);
            r.checked();
            QCOMPARE(r.app.history().recent().size(), 1);
        }
        // and next time
        Running again(folders);
        QCOMPARE(again.app.history().recent().value(0).name, u"Doom Visage"_s);
    }

    void wear()
    {
        // what you say in the popup you wear
        Folders folders;
        Settings settings;
        settings.character = folders.add(wearing(u"Torvald"_s, itemtexts::helmetOld));
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.checked().versus, u"vs Grim Crown"_s);
        r.app.checker().popup().wear(u"Helmet"_s);
        QCOMPARE(r.app.character().character().gear.value(u"Helmet"_s).name, u"Doom Visage"_s);
        QCOMPARE(r.app.characters().find(settings.character)->gear.value(u"Helmet"_s).name, u"Doom Visage"_s);
        QCOMPARE(r.app.checker().popup().report().verdict, u"Equipped"_s);
        QCOMPARE(r.app.checker().popup().wearError(), QString());
    }

    void wearForNobody()
    {
        Folders folders;
        Running r(folders);
        r.checked();
        r.app.checker().popup().wear(u"Helmet"_s);
        QCOMPARE(r.app.checker().popup().wearError(), u"There's no character to wear it. Add one first."_s);
    }

    void gearScoring()
    {
        // which the item in the popup is judged again by
        Folders folders;
        Settings settings;
        settings.character = folders.add(wearing(u"Torvald"_s, itemtexts::helmetOld));
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.checked().gearBuild, u"Life and resistances"_s);
        r.app.gearScoring()->setProfile(1);
        QCOMPARE(r.app.characters().find(settings.character)->buildProfile, u"es-caster"_s);
        QCOMPARE(r.app.checker().popup().report().gearBuild, u"Energy Shield spellcaster"_s);
    }

    void statWeights()
    {
        // which judge the item in the popup when there's no build profile
        Folders folders;
        Settings settings;
        Character c = wearing(u"Torvald"_s, itemtexts::helmetOld);
        c.buildProfile = noBuildProfile.toString();
        settings.character = folders.add(c);
        folders.set(settings);
        Running r(folders);
        const ItemReport before = r.checked();
        QVERIFY(before.scoreLabel.startsWith(u"by your weights"_s));
        r.app.statWeights()->setWeight(u"# to maximum Life"_s, 2);
        QCOMPARE(r.app.characters().find(settings.character)->weights.value(u"# to maximum Life"_s), 2.0);
        QCOMPARE_NE(r.app.checker().popup().report().score, before.score);
    }

    void ignoredStats()
    {
        // which the item in the popup is judged again without
        Folders folders;
        Settings settings;
        Character c = wearing(u"Torvald"_s, itemtexts::helmetOld);
        c.buildProfile = noBuildProfile.toString();
        settings.character = folders.add(c);
        folders.set(settings);
        Running r(folders);
        const ItemReport before = r.checked();
        r.app.ignoredStats()->add(u"# to maximum Life"_s);
        QCOMPARE(r.app.characters().find(settings.character)->ignored.size(), 1);
        QCOMPARE_NE(r.app.checker().popup().report().score, before.score);
    }

    void checkedItems()
    {
        Folders folders;
        Settings settings;
        settings.character = folders.add(wearing(u"Torvald"_s, itemtexts::helmetOld));
        folders.set(settings);
        Running r(folders);
        CheckedItems &checked = *r.app.checkedItems();
        QCOMPARE(checked.rowCount(), 0);
        r.checked();
        QCOMPARE(checked.rowCount(), 1);
        QCOMPARE(checked.index(0).data(CheckedItems::NameRole), u"Doom Visage"_s);

        // another one, then the first again
        r.app.checker().check();
        Q_EMIT r.input->itemCopied(itemtexts::advancedRing, std::nullopt);
        QCOMPARE(checked.index(0).data(CheckedItems::NameRole), u"Storm Loop"_s);
        r.app.checker().popup().close();
        checked.checkAgain(1);
        QVERIFY(r.app.checker().popup().isVisible());
        QCOMPARE(r.app.checker().popup().report().name, u"Doom Visage"_s);
        QCOMPARE(r.app.checker().popup().report().versus, u"vs Grim Crown"_s);
        // which moves it to the top
        QCOMPARE(checked.index(0).data(CheckedItems::NameRole), u"Doom Visage"_s);
        QCOMPARE(checked.rowCount(), 2);

        // for what you wear now
        r.app.checker().popup().wear(u"Helmet"_s);
        r.app.checker().popup().close();
        checked.checkAgain(0);
        QVERIFY(r.app.checker().popup().isVisible());
        QCOMPARE(r.app.checker().popup().report().verdict, u"Equipped"_s);
    }

    void forgetCheckedItems()
    {
        Folders folders;
        {
            Running r(folders);
            r.checked();
            r.app.checkedItems()->forget();
            QCOMPARE(r.app.checkedItems()->rowCount(), 0);
            QVERIFY(r.app.history().recent().isEmpty());
        }
        // and next time
        Running again(folders);
        QCOMPARE(again.app.checkedItems()->rowCount(), 0);
        QVERIFY(!again.app.history().seenStats().isEmpty());
    }

    void judgesWithPob()
    {
        Folders folders;
        Settings settings;
        settings.character = folders.add(wearing(u"Torvald"_s, itemtexts::helmetOld), build);
        settings.pob.location = folders.pob.folder.path();
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.checked().pobChecking);
    }

    void tradeData()
    {
        // and the rates in the league searches go to, once the site has said
        // which leagues there are
        Folders folders;
        Running r(folders);
        QVERIFY(r.rated());
        QCOMPARE(r.asked(), (QList<QUrl>{tradeDataUrl(u"stats"_s), tradeDataUrl(u"filters"_s),
                                         tradeDataUrl(u"leagues"_s), ninjaCurrencyUrl(u"Runes of Aldur"_s)}));
        QVERIFY(r.app.trade().unavailable().isEmpty());
        QCOMPARE(r.app.trade().league().tradeId(), u"Runes of Aldur"_s);
        QCOMPARE(r.app.tradeData().data().stats.all().size(), 7);
        QCOMPARE(r.app.rates().divineRate(), 400);
        // which are kept in the cache
        QVERIFY(QFile::exists(folders.dir.filePath(u"cache/trade/stats.json"_s)));
        QVERIFY(QFile::exists(folders.dir.filePath(u"cache/rates/Runes of Aldur.json"_s)));
    }

    void hardcore()
    {
        Folders folders;
        Settings settings;
        settings.trade.league.hardcore = true;
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.rated());
        QCOMPARE(r.app.trade().league().tradeId(), u"HC Runes of Aldur"_s);
        QCOMPARE(r.asked().last(), ninjaCurrencyUrl(u"HC Runes of Aldur"_s));
    }

    void tradeOff()
    {
        Folders folders;
        Settings settings;
        settings.trade.enabled = false;
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.app.trade().unavailable(), u"Trade searches are turned off in the settings."_s);
        // and neither site is asked anything
        QVERIFY(r.asked().isEmpty());
        // nor has the popup buttons for it
        QVERIFY(!r.app.checker().popup().isTradeAvailable());

        // until it's on
        settings.trade.enabled = true;
        QVERIFY(r.app.settings().set(settings));
        QVERIFY(r.rated());
        QCOMPARE(r.asked().size(), 4);
        QVERIFY(r.app.trade().unavailable().isEmpty());
        QVERIFY(r.app.checker().popup().isTradeAvailable());
    }

    void statPicker()
    {
        // which only asks the trade site for its stats while there are
        // trade searches
        Folders folders;
        Settings settings;
        settings.trade.league.soloSelfFound = true;
        folders.set(settings);
        // empty, with room for more requests than it guesses there is
        auto *empty = new FakeNetwork;
        const FakeAnswer nothing{.headers = {{"x-rate-limit-ip", "20:10:0"}}};
        empty->answers = {nothing, nothing, nothing};
        Running r(folders, desktop(), empty);
        StatPicker *picker = r.app.statPicker();
        picker->refresh();
        QVERIFY(r.asked().isEmpty());
        QCOMPARE(picker->rowCount(), equipmentKeys().size());

        settings.trade.league.soloSelfFound = false;
        QVERIFY(r.app.settings().set(settings));
        QTest::ignoreMessage(QtWarningMsg, "The trade site's stats came back empty");
        QTest::ignoreMessage(QtWarningMsg, "The trade site's filters came back empty");
        QTest::ignoreMessage(QtWarningMsg, "The trade site's leagues came back empty");
        // which with no leagues are for Standard
        QTest::ignoreMessage(QtWarningMsg, "Couldn't get currency rates for Standard from poe.ninja: "
                                           "poe.ninja has no Divine Orb price for Standard.");
        QSignalSpy refreshed(&r.app.tradeData(), &TradeDataStore::refreshed);
        QVERIFY(refreshed.wait());
        QVERIFY(r.app.tradeData().data().stats.isEmpty());

        // and again when it didn't get them
        r.network->answers = siteAnswers();
        QSignalSpy reset(picker, &StatPicker::modelReset);
        picker->refresh();
        QVERIFY(reset.wait());
        QCOMPARE(picker->rowCount(), equipmentKeys().size() + 4);
    }

    void offMeanwhile()
    {
        // before the site has answered, so poe.ninja isn't asked
        Folders folders;
        Running r(folders);
        Settings settings;
        settings.trade.enabled = false;
        QVERIFY(r.app.settings().set(settings));
        QSignalSpy refreshed(&r.app.tradeData(), &TradeDataStore::refreshed);
        QVERIFY(refreshed.wait());
        QCOMPARE(r.asked().size(), 3);
    }

    void soloSelfFound()
    {
        Folders folders;
        Settings settings;
        settings.trade.league.soloSelfFound = true;
        folders.set(settings);
        Running r(folders);
        QCOMPARE(r.app.trade().unavailable(),
                 u"There's no trading in solo self-found, so trade searches are off."_s);
        QVERIFY(r.asked().isEmpty());
        QVERIFY(!r.app.checker().popup().isTradeAvailable());
    }

    void findsOnTrade()
    {
        // for the character you play, in your budget at poe.ninja's rate
        Folders folders;
        Character character;
        character.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(character);
        settings.trade.budget.max = 1;
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.rated());
        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}};
        // from the popup's button
        r.checked();
        ItemPopup &popup = r.app.checker().popup();
        QVERIFY(popup.isTradeAvailable());
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        popup.findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QVERIFY(r.app.finder().errorString().isEmpty());
        QCOMPARE(r.app.finder().found()->total, 2);
        QCOMPARE(popup.trade().status, u"2 listed in Runes of Aldur"_s);
        // by your weights, which the site has filters for, unlike the usual ones
        QVERIFY(r.app.finder().found()->note.isEmpty());

        const FakeNetwork::Sent &search = r.network->sent.last();
        QCOMPARE(search.request.url(), tradeSearchUrl(u"Runes of Aldur"_s));
        const QJsonObject query = QJsonDocument::fromJson(search.body).object().value(u"query").toObject();
        const QJsonObject trade = query.value(u"filters").toObject().value(u"trade_filters").toObject();
        QCOMPARE(trade.value(u"filters").toObject().value(u"price").toObject(),
                 (QJsonObject{{u"max"_s, 400}, {u"option"_s, u"exalted"_s}}));
    }

    void findsBeforeTheSiteHasAnswered()
    {
        // the first time, so the search waits for the site's data and goes to the
        // league it names, with the rates for that league and no other
        Folders folders;
        Character character;
        character.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(character);
        folders.set(settings);
        FakeNetwork *sites = online();
        sites->answers << FakeAnswer{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"};
        Running r(folders, desktop(), sites);
        r.checked();
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QVERIFY(r.app.finder().errorString().isEmpty());
        QCOMPARE(r.app.finder().found()->league, u"Runes of Aldur"_s);
        QCOMPARE(r.asked(), (QList<QUrl>{tradeDataUrl(u"stats"_s), tradeDataUrl(u"filters"_s),
                                         tradeDataUrl(u"leagues"_s), ninjaCurrencyUrl(u"Runes of Aldur"_s),
                                         tradeSearchUrl(u"Runes of Aldur"_s)}));
    }

    void tradeResults()
    {
        // the listings of what the popup searched for, which the site is only
        // asked for once you want to see them
        Folders folders;
        Character character;
        character.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(character);
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.rated());
        TradeResults &results = r.app.results();
        QVERIFY(!results.hasSearched());

        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}};
        r.checked();
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QVERIFY(results.hasSearched());
        QCOMPARE(results.total(), 2);
        QCOMPARE(results.league(), u"Runes of Aldur"_s);
        QCOMPARE(results.url(), r.app.finder().found()->url);
        QCOMPARE(results.rowCount(), 0);
        const QUrl fetch = tradeFetchUrl({u"a"_s, u"b"_s}, u"Ab3dE9"_s);
        QVERIFY(!r.asked().contains(fetch));

        r.network->answers = {twoListings()};
        results.ask();
        QTRY_VERIFY(!results.isBusy());
        QVERIFY(r.asked().contains(fetch));
        QCOMPARE(results.rowCount(), 2);
        QCOMPARE(results.data(results.index(0), TradeResults::PriceRole).toString(), u"3 div"_s);
        QCOMPARE(results.data(results.index(1), TradeResults::ConvertedRole).toString(), u"about 1200 ex"_s);
    }

    void tradeResultsJudged()
    {
        // for the character you play, with Path of Building while it's on
        Folders folders;
        Character torvald = wearing(u"Torvald"_s, itemtexts::helmetOld);
        torvald.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(torvald, build);
        const QString astrid = folders.add(wearing(u"Astrid"_s, itemtexts::helmetNew));
        settings.pob.location = folders.pob.folder.path();
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.rated());
        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}};
        r.checked();
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        TradeResults &results = r.app.results();
        r.network->answers = {twoListings()};
        results.ask();
        QTRY_VERIFY(!results.isBusy());

        const QList<ItemCheck> &checks = results.judge().checks();
        QCOMPARE(checks.size(), 2);
        QCOMPARE(checks.at(0).evaluation.comparison.replacing, QStringList{u"Grim Crown"_s});
        QVERIFY(checks.at(0).pob != ItemCheck::Pob::Unused);
        settings.pob.enabled = false;
        QVERIFY(r.app.settings().set(settings));
        QCOMPARE(checks.at(0).pob, ItemCheck::Pob::Unused);

        settings.character = astrid;
        QVERIFY(r.app.settings().set(settings));
        QCOMPARE(checks.at(0).evaluation.comparison.replacing, QStringList{u"Doom Visage"_s});
    }

    void resultsStayWhenASearchFails()
    {
        Folders folders;
        Character character;
        character.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(character);
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.rated());
        r.checked();
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}};
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());

        // a search that finds nothing is what they have
        r.network->answers = {{.body = R"({"id":"Qw8rT2","total":0,"result":[]})"}};
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QCOMPARE(r.app.results().total(), 0);
        QVERIFY(!r.app.results().hasMore());

        // but one that fails isn't
        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}};
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QCOMPARE(r.app.results().total(), 2);
        r.network->answers = {{.status = 400, .body = R"({"error":{"message":"Nope."}})"}};
        QTest::ignoreMessage(QtWarningMsg, "Couldn't search the trade site: The trade site says: Nope.");
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QVERIFY(!r.app.finder().found());
        QCOMPARE(r.app.results().total(), 2);
        QVERIFY(r.app.results().hasMore());
    }

    void noResultsWhileTradeIsOff()
    {
        // so the site isn't asked for the listings, however you ask
        Folders folders;
        Character character;
        character.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(character);
        folders.set(settings);
        Running r(folders);
        QVERIFY(r.rated());
        r.checked();
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}};
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());

        settings.trade.enabled = false;
        QVERIFY(r.app.settings().set(settings));
        const qsizetype before = r.network->sent.size();
        r.app.results().ask();
        QVERIFY(!r.app.results().isBusy());
        QCOMPARE(r.app.results().error(), r.app.trade().unavailable());
        QCOMPARE(r.network->sent.size(), before);
    }

    void signsInAtStart()
    {
        Folders folders;
        Character character;
        character.weights = {{u"# to maximum Life"_s, 1}};
        Settings settings;
        settings.character = folders.add(character);
        folders.set(settings);
        Running r(folders, desktop(), online(), keyringWith(fakeId));
        QVERIFY(r.rated());
        QVERIFY(r.signedIn());
        QVERIFY(r.app.tradeSession()->isSaved());
        QCOMPARE(r.keyring->reads, 1);

        // so the searches have it
        r.network->answers = {{.body = R"({"id":"Ab3dE9","total":2,"result":["a","b"]})"}};
        r.checked();
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        r.app.checker().popup().findOnTrade(TradeFinder::Kind::Weighted);
        QVERIFY(finished.wait());
        QCOMPARE(r.network->sent.last().request.url(), tradeSearchUrl(u"Runes of Aldur"_s));
        QCOMPARE(r.network->sent.last().request.rawHeader("Cookie"), "POESESSID=" + fakeId.toLatin1());
    }

    void keyringWhileTradeIsOn()
    {
        // which is only asked once, and not while there are no searches
        Folders folders;
        Settings settings;
        settings.trade.enabled = false;
        folders.set(settings);
        Running r(folders, desktop(), online(), keyringWith(fakeId));
        QVERIFY(r.signedIn());
        QCOMPARE(r.keyring->reads, 0);
        QVERIFY(!r.app.tradeSession()->isSaved());

        settings.trade.enabled = true;
        QVERIFY(r.app.settings().set(settings));
        QVERIFY(r.signedIn());
        QVERIFY(r.app.tradeSession()->isSaved());
        QCOMPARE(r.keyring->reads, 1);

        // and not again at the next change
        settings.trade.enabled = false;
        QVERIFY(r.app.settings().set(settings));
        settings.trade.enabled = true;
        QVERIFY(r.app.settings().set(settings));
        QVERIFY(r.signedIn());
        QCOMPARE(r.keyring->reads, 1);
    }

    void noKeyringInSoloSelfFound()
    {
        Folders folders;
        Settings settings;
        settings.trade.league.soloSelfFound = true;
        folders.set(settings);
        Running r(folders, desktop(), online(), keyringWith(fakeId));
        QVERIFY(r.signedIn());
        QCOMPARE(r.keyring->reads, 0);
    }

    void signingInSearchesBigger()
    {
        // as signed out, the site takes only the smallest upgrade searches
        Folders folders;
        Settings settings;
        settings.character = folders.add(wearing(u"Torvald"_s, itemtexts::helmetOld));
        folders.set(settings);
        Running r(folders, desktop(), online(tradefixtures::manyStats()));
        QVERIFY(r.rated());
        QVERIFY(r.signedIn());
        r.checked();
        ItemPopup &popup = r.app.checker().popup();
        QSignalSpy finished(&r.app.finder(), &TradeFinder::finished);
        const qsizetype before = r.network->sent.size();
        r.network->answers = {{.body = R"({"id":"s1","total":3,"result":[]})"}};
        popup.findOnTrade(TradeFinder::Kind::Upgrades);
        QVERIFY(finished.wait());
        QCOMPARE(r.network->sent.size(), before + 1);
        const QByteArray smallest = r.network->sent.last().body;
        QCOMPARE(r.network->sent.last().request.rawHeader("Cookie"), QByteArray());
        QVERIFY(r.app.finder().found()->note.startsWith(u"Signed out, "));

        // until you sign in
        r.app.tradeSession()->save(fakeId);
        QVERIFY(r.signedIn());
        QCOMPARE(r.keyring->secrets.value(u"POESESSID"_s), fakeId);
        r.network->answers = {{.body = R"({"id":"s2","total":3,"result":[]})"}};
        popup.findOnTrade(TradeFinder::Kind::Upgrades);
        QVERIFY(finished.wait());
        QCOMPARE(r.network->sent.size(), before + 2);
        QVERIFY(r.network->sent.last().body != smallest);
        QCOMPARE(r.network->sent.last().request.rawHeader("Cookie"), "POESESSID=" + fakeId.toLatin1());

        // and out again
        r.app.tradeSession()->forget();
        QVERIFY(r.signedIn());
        r.network->answers = {{.body = R"({"id":"s3","total":3,"result":[]})"}};
        popup.findOnTrade(TradeFinder::Kind::Upgrades);
        QVERIFY(finished.wait());
        QCOMPARE(r.network->sent.size(), before + 3);
        QCOMPARE(r.network->sent.last().body, smallest);
    }

    void signingOut()
    {
        Folders folders;
        Running r(folders, desktop(), online(), keyringWith(fakeId));
        QVERIFY(r.signedIn());
        QVERIFY(r.app.tradeSession()->isSaved());
        r.app.tradeSession()->forget();
        QVERIFY(r.signedIn());
        QVERIFY(!r.app.tradeSession()->isSaved());
        QVERIFY(r.keyring->secrets.isEmpty());
    }

    void keyringIsLocked()
    {
        // which doesn't hold the app up, and is told on the trade page
        Folders folders;
        auto *locked = keyringWith(fakeId);
        locked->failure = u"The keyring is locked."_s;
        Running r(folders, desktop(), online(), locked);
        QVERIFY(r.rated());
        QVERIFY(r.signedIn());
        QVERIFY(!r.app.tradeSession()->isSaved());
        QCOMPARE(r.app.tradeSession()->error(), u"The keyring is locked."_s);
        QVERIFY(r.app.trade().unavailable().isEmpty());
    }
};

QTEST_MAIN(TestApp)
#include "tst_app.moc"
