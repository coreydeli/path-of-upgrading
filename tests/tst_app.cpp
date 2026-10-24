// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "core/parser.h"
#include "fakegameinput.h"
#include "fakepob.h"
#include "itemtexts.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>
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
    App::Paths paths{.settings = dir.filePath(u"settings.conf"_s), .data = dir.filePath(u"data"_s)};
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

// The app with a stand-in for the game.
struct Running
{
    QQmlEngine qml;
    FakeGameInput *input = new FakeGameInput;
    App app;

    explicit Running(const Folders &folders)
        : app(&qml, std::unique_ptr<GameInput>(input), folders.places(), folders.paths)
    {}

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
};

QTEST_MAIN(TestApp)
#include "tst_app.moc"
