// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "fakegameinput.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakepob.h"
#include "tradefixtures.h"

#include <KLocalizedQmlContext>
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
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
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

bool makePob(const QString &folder)
{
    QFile wrapper(folder + u"/HeadlessWrapper.lua"_s);
    return wrapper.open(QIODevice::WriteOnly);
}

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
        QObject *found = page()->findChild<QObject *>(name);
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
        QCOMPARE(w.page()->property("title").toString(), u"Path of Building"_s);
    }

    void pobNotFound()
    {
        Window w;
        QObject *found = w.find(u"foundText"_s);
        QCOMPARE(text(found), u"Path of Building wasn't found."_s);
        QVERIFY(found->property("description").toString().startsWith(u"Path of Building isn't installed."_s));
    }

    void pobFolder()
    {
        Window w;
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
