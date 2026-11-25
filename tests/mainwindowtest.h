// SPDX-License-Identifier: MIT
// What the main window's tests share: the window with the app in it, and
// the helpers for finding things in it and for what a person does there.
#pragma once

#include "app/app.h"
#include "core/parser.h"
#include "fakegameinput.h"
#include "fakegamewatcher.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "fakepob.h"
#include "fakesecretstore.h"
#include "services/datafiles.h"
#include "tradefixtures.h"

#include <KLocalizedQmlContext>
#include <QAccessible>
#include <QDir>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtQuickTest/quicktest.h>
#include <functional>
#include <memory>
#include <optional>

namespace pou::windowtest {

using namespace Qt::Literals::StringLiterals;

// Made up, and not a real one
inline const QString fakeId = u"0123456789abcdef0123456789abcdef"_s;

inline QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// The trade site with its data and leagues, and poe.ninja with what
// currencies are worth, each said to whoever asks, whatever the order.
inline FakeNetwork *online(bool ratesCome = true)
{
    QJsonArray leagues;
    for (const QString &id : {u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s, u"Hardcore"_s})
        leagues << QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}};
    auto *network = new FakeNetwork;
    network->route = [leagues, ratesCome](const QNetworkRequest &request) -> std::optional<FakeAnswer> {
        const QString path = request.url().path();
        if (path.endsWith(u"/data/stats"_s))
            return FakeAnswer{.body = json(tradefixtures::stats())};
        if (path.endsWith(u"/data/filters"_s))
            return FakeAnswer{.body = json(tradefixtures::filters())};
        if (path.endsWith(u"/data/leagues"_s))
            return FakeAnswer{.body = json(QJsonObject{{u"result"_s, leagues}})};
        if (request.url().host().contains(u"poe.ninja"_s)) {
            if (!ratesCome)
                return FakeAnswer{.never = true};
            return FakeAnswer{.body = R"({"core":{"primary":"divine","rates":{"exalted":400}},"lines":[]})"};
        }
        return std::nullopt;
    };
    return network;
}

// Through the children and the items in them both, as what a Repeater makes
// is only in the items.
inline QObject *search(QObject *object, const std::function<bool(QObject *)> &wanted, QSet<QObject *> &seen)
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

inline QObject *search(QObject *object, const std::function<bool(QObject *)> &wanted)
{
    QSet<QObject *> seen;
    return search(object, wanted, seen);
}

// Every one with the name, in the order they were laid out.
inline QList<QObject *> findAll(QObject *object, const QString &name)
{
    QList<QObject *> found;
    QSet<QObject *> seen;
    search(
        object,
        [&](QObject *o) {
            if (o->objectName() == name)
                found << o;
            return false;
        },
        seen);
    return found;
}

// what a dialog has in it by that name
inline QObject *inDialog(QObject *dialog, const QString &name)
{
    return dialog ? search(dialog, [&](QObject *o) { return o->objectName() == name; }) : nullptr;
}

// one of a dialog's buttons, by what it says
inline QObject *button(QObject *dialog, const QString &text)
{
    return dialog ? search(dialog,
                           [&](QObject *o) {
                               return o->inherits("QQuickAbstractButton") &&
                                      o->property("text").toString() == text;
                           })
                  : nullptr;
}

inline bool makePob(const QString &folder)
{
    QFile wrapper(folder + u"/HeadlessWrapper.lua"_s);
    return wrapper.open(QIODevice::WriteOnly);
}

inline Item item(const QString &text)
{
    return parseItem(text).value();
}

inline const QString jewel =
    u"Item Class: Jewels\nRarity: Rare\nPale Eye\nRuby\n--------\n+8% to Fire Resistance"_s;
inline const QString luckyJewel = u"Item Class: Jewels\nRarity: Rare\nGrim Spark\nEmerald\n--------\n"
                                  "+8% to Fire Resistance\n10% increased Rarity of Items found"_s;

// a build with nothing on
inline const QString ranger =
    u"<PathOfBuilding2><Build level=\"40\" className=\"Ranger\"/></PathOfBuilding2>"_s;

// The settings of someone who has been through the setup guide, with trade
// searches on, which only a beta starts with.
inline Settings used()
{
    Settings settings;
    settings.setupDone = true;
    return settings;
}

// The app with its window, and stand-ins for the game, the desktop and the
// sites. Path of Building isn't in any of the usual places.
struct Window
{
    QTemporaryDir dir;
    FakePob pob;
    QQmlEngine qml;
    FakeGlobalShortcuts *shortcuts = new FakeGlobalShortcuts;
    FakeSecretStore *keyring = new FakeSecretStore;
    FakeOpener *opener = new FakeOpener;
    FakeNetwork *network;
    std::unique_ptr<App> app;
    std::unique_ptr<QQuickWindow> window;

    // The app's colors are only on when there's a scheme file for them, as
    // they go on the whole program.
    // The settings are saved before it starts, or none for the first run.
    // It has trade, which a release build of the app leaves out.
    explicit Window(bool hotkeys = true, bool startHidden = false, const QString &colors = {},
                    const std::optional<Settings> &saved = used(), bool tradeOffered = true,
                    FakeGameWatcher *game = nullptr, bool siteKept = true, bool ratesCome = true,
                    bool startOverOffered = true)
        : network(online(ratesCome))
    {
        shortcuts->available = hotkeys;
        if (siteKept)
            keepSite();
        if (saved)
            SettingsStore(dir.filePath(u"settings.conf"_s)).set(*saved);
        app = std::make_unique<App>(
            &qml, std::make_unique<FakeGameInput>(), std::unique_ptr<GlobalShortcuts>(shortcuts),
            std::unique_ptr<SecretStore>(keyring), std::unique_ptr<Opener>(opener),
            std::unique_ptr<QNetworkAccessManager>(network),
            PobPlaces{.home = dir.path(), .dataHome = dir.path(), .luajit = QStringLiteral(FAKEPOB)},
            App::Paths{.settings = dir.filePath(u"settings.conf"_s),
                       .data = dir.filePath(u"data"_s),
                       .cache = dir.filePath(u"cache"_s),
                       .autostart = dir.filePath(u"autostart/made-up.desktop"_s),
                       .colors = colors,
                       .tradeOffered = tradeOffered,
                       .startOverOffered = startOverOffered},
            std::unique_ptr<GameWatcher>(game));
        // whatever the build was configured for, a test says what it needs
        app->updates()->setInstallKind(InstallKind::Source);
        KLocalization::setupLocalizedContext(&qml);
        QQmlComponent main(&qml, u"PathOfUpgrading"_s, u"Main"_s);
        QVariantMap properties{{u"app"_s, QVariant::fromValue(app.get())}};
        // which it isn't unless it's said to be
        if (startHidden)
            properties.insert(u"startHidden"_s, true);
        QObject *made = main.createWithInitialProperties(properties);
        if (!made)
            qWarning() << main.errorString();
        window.reset(qobject_cast<QQuickWindow *>(made));
    }

    // What the site said on an earlier run, which is all the app goes by
    // until a search or Refresh asks it again.
    void keepSite() const
    {
        const QString trade = dir.filePath(u"cache/trade"_s);
        QJsonArray leagues;
        for (const QString &id : {u"Runes of Aldur"_s, u"HC Runes of Aldur"_s, u"Standard"_s, u"Hardcore"_s})
            leagues << QJsonObject{{u"id"_s, id}, {u"realm"_s, u"poe2"_s}};
        if (!QDir().mkpath(trade) || !writeJsonFile(trade + u"/stats.json"_s, tradefixtures::stats()) ||
            !writeJsonFile(trade + u"/filters.json"_s, tradefixtures::filters()) ||
            !writeJsonFile(trade + u"/leagues.json"_s, QJsonObject{{u"result"_s, leagues}}))
            qWarning() << "Couldn't keep the site's data in" << trade;
    }

    // The rows a list let go of are only deleted later, and in Qt 6.8 a QML
    // object that outlives its engine leaves its types for the next engine,
    // which can't use them.
    ~Window()
    {
        // for the next test's colors
        app->appearance()->setColors(u"desktop"_s);
        window.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    const Settings &settings() const { return app->settings().settings(); }

    // as when you pick it in the sidebar
    bool show(const QString &action) const
    {
        QObject *found = window->findChild<QObject *>(action);
        return found && QMetaObject::invokeMethod(found, "trigger");
    }

    QObject *drawer() const { return window->property("globalDrawer").value<QObject *>(); }

    // Kirigami 6.13 puts an & before the key that picks a row, and has its own
    // type for the rows besides the button for Back.
    static QString plain(QString text) { return text.remove(u'&'); }

    static bool isRow(const QObject *item)
    {
        return item->inherits("QQuickAbstractButton") ||
               QByteArrayView(item->metaObject()->className()).contains("GlobalDrawerActionItem");
    }

    // The row in the sidebar with that text, if it's showing. Kirigami's
    // rows are all the same type, so it goes by the label in them.
    QQuickItem *sidebarRow(const QString &text) const
    {
        const auto *label = qobject_cast<QQuickItem *>(search(drawer(), [&](QObject *o) {
            return o->metaObject()->indexOfProperty("elide") >= 0 && o->property("visible").toBool() &&
                   plain(o->property("text").toString()) == text;
        }));
        for (QQuickItem *up = label ? label->parentItem() : nullptr; up; up = up->parentItem()) {
            if (isRow(up))
                return up;
        }
        return nullptr;
    }

    // as you do with the mouse
    bool clickRow(const QString &text) const
    {
        QQuickItem *row = sidebarRow(text);
        if (!row)
            return false;
        QTest::mouseClick(window.get(), Qt::LeftButton, {},
                          row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
        return true;
    }

    // The text of the sidebar row with the keyboard focus.
    QString focusedRow() const
    {
        for (QQuickItem *up = window->activeFocusItem(); up; up = up->parentItem()) {
            if (isRow(up)) {
                const QObject *label =
                    search(up, [](QObject *o) { return o->metaObject()->indexOfProperty("elide") >= 0; });
                return label ? plain(label->property("text").toString()) : QString();
            }
        }
        return QString();
    }

    // the setup guide's dialog
    QObject *guide() const { return window->findChild<QObject *>(u"setupGuide"_s); }

    // what it has by that name
    QObject *inGuide(const QString &name) const
    {
        QObject *found = search(guide(), [&](QObject *o) { return o->objectName() == name; });
        if (!found)
            qWarning() << "There's no" << name << "in the guide";
        return found;
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

    // On the offscreen screen and laid out, as a list only makes the rows it
    // shows once it's laid out, and Kirigami 6.13 keeps a page disabled until
    // it's laid out in view.
    bool shown() const
    {
        window->resize(1000, 700);
        window->show();
        return QTest::qWaitForWindowExposed(window.get()) && settled();
    }

    // once what changed is laid out
    bool settled() const { return QQuickTest::qWaitForPolish(window.get()); }
};

// What clicking a switch does.
inline bool toggle(QObject *control)
{
    return control && control->setProperty("checked", !control->property("checked").toBool()) &&
           QMetaObject::invokeMethod(control, "toggled");
}

// Picking one of a combo box's choices.
inline bool activate(QObject *box, int index)
{
    return box && box->setProperty("currentIndex", index) &&
           QMetaObject::invokeMethod(box, "activated", Q_ARG(int, index));
}

inline bool setValue(QObject *control, const QVariant &value, const char *signal)
{
    return control && control->setProperty("value", value) && QMetaObject::invokeMethod(control, signal);
}

inline bool pressKey(QObject *recorder, const QKeySequence &key)
{
    return recorder && recorder->setProperty("keySequence", key) &&
           QMetaObject::invokeMethod(recorder, "keySequenceModified");
}

// What typing it into a NumberField's spin box comes to, in the box's units.
inline int typed(QObject *spinBox, const QString &text)
{
    const auto fromText = spinBox->property("valueFromText").value<QJSValue>();
    return fromText.call({text, qjsEngine(spinBox)->toScriptValue(QLocale())}).toInt();
}

inline QString text(QObject *item)
{
    return item ? item->property("text").toString() : QString();
}

inline bool click(QObject *button)
{
    return button && QMetaObject::invokeMethod(button, "clicked");
}

// A key pressed in a field, as though it had the focus.
inline bool press(QObject *field, Qt::Key key)
{
    if (!field)
        return false;
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(field, &event);
    return true;
}

inline bool isVisible(QObject *item)
{
    return item && item->property("visible").toBool();
}

// What a screen reader calls it.
inline QString spoken(QObject *item)
{
    const QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(item);
    return accessible ? accessible->text(QAccessible::Name) : QString();
}

} // namespace pou::windowtest
