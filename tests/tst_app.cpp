// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "core/parser.h"
#include "core/rates.h"
#include "core/tradesite.h"
#include "fakegameinput.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakepob.h"
#include "itemtexts.h"
#include "tradefixtures.h"

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
                     .cache = dir.filePath(u"cache"_s)};
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

// The app with stand-ins for the game, the desktop and the sites it asks.
struct Running
{
    QQmlEngine qml;
    FakeGameInput *input = new FakeGameInput;
    FakeGlobalShortcuts *shortcuts;
    FakeNetwork *network;
    App app;

    explicit Running(const Folders &folders, FakeGlobalShortcuts *theirs = desktop(),
                     FakeNetwork *sites = online())
        : shortcuts(theirs)
        , network(sites)
        , app(&qml, std::unique_ptr<GameInput>(input), std::unique_ptr<GlobalShortcuts>(shortcuts),
              std::unique_ptr<QNetworkAccessManager>(network), folders.places(), folders.paths)
    {}

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

        // until it's on
        settings.trade.enabled = true;
        QVERIFY(r.app.settings().set(settings));
        QVERIFY(r.rated());
        QCOMPARE(r.asked().size(), 4);
        QVERIFY(r.app.trade().unavailable().isEmpty());
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
    }
};

QTEST_MAIN(TestApp)
#include "tst_app.moc"
