// SPDX-License-Identifier: MIT
//
// The card on the Characters page for linking a Path of Exile account, in the
// real window, with a fake website.
#include "app/accountcharacters.h"
#include "app/app.h"
#include "app/fonts.h"
#include "app/poeaccount.h"
#include "fakegameinput.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "fakepob.h"
#include "fakesecretstore.h"
#include "stylefortests.h"

#include <KLocalizedQmlContext>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQuickTest/quicktest.h>
#include <functional>
#include <memory>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// made-up
const PoeConfig registered{.clientId = u"examplepou"_s, .contact = u"example.invalid"_s, .ports = {49082}};

// and with what the build needs to search the trade site as the account
const PoeConfig registeredForTrade{.clientId = u"examplepou"_s,
                                   .contact = u"example.invalid"_s,
                                   .ports = {49082},
                                   .tradeScope = u"account:made_up_trade"_s,
                                   .tradeUrl = u"https://api.example.invalid/trade"_s};

const QByteArray list =
    R"({"characters":[)"
    R"({"id":"aa","name":"FakeOne","class":"Stormweaver","league":"Standard","level":92,"current":true},)"
    R"({"id":"bb","name":"FakeTwo","class":"Ranger","league":"HC Fake League","level":7}]})";

const QByteArray wearing =
    R"({"character":{"id":"aa","name":"FakeOne","class":"Stormweaver","level":92,"equipment":[)"
    R"({"inventoryId":"Helm","name":"Fake Crown","typeLine":"Iron Hat","baseType":"Iron Hat","rarity":"Rare",)"
    R"("explicitMods":["+45 to maximum Life"]}]}})";

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

QList<QObject *> findAll(QObject *object, const QString &name)
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

bool isVisible(QObject *item)
{
    return item && item->property("visible").toBool();
}

bool click(QObject *button)
{
    return button && QMetaObject::invokeMethod(button, "clicked");
}

// The app with its window, a keyring, and a website that has the account's
// characters. The account is linked when the keyring has the link.
struct Window
{
    QTemporaryDir dir;
    FakePob pob;
    QQmlEngine qml;
    FakeSecretStore *keyring = new FakeSecretStore;
    FakeNetwork *network = new FakeNetwork;
    std::unique_ptr<App> app;
    std::unique_ptr<QQuickWindow> window;
    FakeAnswer oneAnswer{.body = wearing};

    explicit Window(bool offered = true, bool linked = false, const PoeConfig &config = registered,
                    bool tradeOffered = false)
    {
        network->route = [this](const QNetworkRequest &request) -> std::optional<FakeAnswer> {
            const QString host = request.url().host();
            if (host == u"api.pathofexile.com"_s)
                return request.url().path().count(u'/') > 2 ? oneAnswer : FakeAnswer{.body = list};
            if (host == u"www.pathofexile.com"_s) {
                const QByteArray body = QJsonDocument(QJsonObject{{u"access_token"_s, u"fake-access"_s},
                                                                  {u"refresh_token"_s, u"fake-refresh-2"_s},
                                                                  {u"expires_in"_s, 36000},
                                                                  {u"token_type"_s, u"bearer"_s}})
                                            .toJson(QJsonDocument::Compact);
                return FakeAnswer{.body = body};
            }
            // nothing else is asked for
            return FakeAnswer{.never = true};
        };
        if (linked) {
            const QJsonObject kept{
                {u"refresh"_s, u"fake-refresh"_s},
                {u"until"_s, QDateTime::currentDateTimeUtc().addDays(3).toString(Qt::ISODate)}};
            keyring->secrets.insert(PoeAccount::secretName(),
                                    QString::fromUtf8(QJsonDocument(kept).toJson(QJsonDocument::Compact)));
        }
        Settings used;
        used.setupDone = true;
        SettingsStore(dir.filePath(u"settings.conf"_s)).set(used);
        app = std::make_unique<App>(
            &qml, std::make_unique<FakeGameInput>(), std::make_unique<FakeGlobalShortcuts>(),
            std::unique_ptr<SecretStore>(keyring), std::make_unique<FakeOpener>(),
            std::unique_ptr<QNetworkAccessManager>(network),
            PobPlaces{.home = dir.path(), .dataHome = dir.path(), .luajit = QStringLiteral(FAKEPOB)},
            App::Paths{.settings = dir.filePath(u"settings.conf"_s),
                       .data = dir.filePath(u"data"_s),
                       .cache = dir.filePath(u"cache"_s),
                       .autostart = dir.filePath(u"autostart/made-up.desktop"_s),
                       .colors = {},
                       .tradeOffered = tradeOffered,
                       .mapsOffered = false,
                       .startOverOffered = false,
                       .poeOffered = offered,
                       .poe = config});
        app->updates()->setInstallKind(InstallKind::Source);
        KLocalization::setupLocalizedContext(&qml);
        QQmlComponent main(&qml, u"PathOfUpgrading"_s, u"Main"_s);
        QObject *made = main.createWithInitialProperties({{u"app"_s, QVariant::fromValue(app.get())}});
        if (!made)
            qWarning() << main.errorString();
        window.reset(qobject_cast<QQuickWindow *>(made));
    }

    // Without Path of Building, which is what the fake's folder is missing.
    ~Window()
    {
        window.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

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

    bool shown() const
    {
        window->resize(1000, 700);
        window->show();
        return QTest::qWaitForWindowExposed(window.get()) && QQuickTest::qWaitForPolish(window.get());
    }
};

} // namespace

class TestAccountCard : public QObject
{
    Q_OBJECT

public:
    static void initMain()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        useStyleInTests();
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(addBundledFonts());
    }

    void init() { QTest::failOnWarning(); }

    void theTradePageOffersTheAccountOnlyWhenTheBuildCan()
    {
        // an account, but no scope or address for trade searches
        Window without(true, false, registered, true);
        QVERIFY(without.show(u"tradeAction"_s));
        QVERIFY(without.shown());
        QVERIFY(isVisible(without.find(u"sessionStatus"_s)));
        QVERIFY(!isVisible(without.find(u"signInBox"_s)));
        QVERIFY(!isVisible(without.find(u"signInStatus"_s)));
        // and not in a build that doesn't offer the account at all
        Window release(false, false, registeredForTrade, true);
        QVERIFY(release.show(u"tradeAction"_s));
        QVERIFY(release.shown());
        QVERIFY(!isVisible(release.find(u"signInBox"_s)));
    }

    void theTradePageLetsYouPickTheAccount()
    {
        Window w(true, true, registeredForTrade, true);
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.shown());
        QObject *box = w.find(u"signInBox"_s);
        QVERIFY(isVisible(box));
        QCOMPARE(box->property("currentIndex").toInt(), 0);
        QCOMPARE(w.find(u"signInStatus"_s)->property("text").toString(),
                 u"Searches are made with your session ID."_s);
        // the session ID is still there to fill in
        QVERIFY(isVisible(w.find(u"sessionStatus"_s)));

        box->setProperty("currentIndex", 1);
        QVERIFY(QMetaObject::invokeMethod(box, "activated", Q_ARG(int, 1)));
        QVERIFY(w.app->tradeSignIn()->useAccount());
        QCOMPARE(SettingsStore(w.dir.filePath(u"settings.conf"_s)).settings().trade.signIn,
                 TradeSignInKind::Account);
        QTRY_COMPARE(w.find(u"signInStatus"_s)->property("text").toString(),
                     u"Your Path of Exile account isn't linked yet. Link it on the Characters page."_s);
    }

    void nothingShowsWhenItIsNotOffered()
    {
        Window w(false);
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.shown());
        QVERIFY(!isVisible(w.find(u"accountHeader"_s)));
        QVERIFY(!isVisible(w.find(u"accountCard"_s)));
        QVERIFY(!isVisible(w.find(u"linkButton"_s)));
        // and the keyring isn't asked about it
        QCOMPARE(w.keyring->reads, 0);
    }

    void offersToLinkWhenNothingIsLinked()
    {
        Window w;
        // it isn't asked until the card is looked at
        QCOMPARE(w.keyring->reads, 0);
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.shown());
        QTRY_COMPARE(w.keyring->reads, 1);
        QVERIFY(isVisible(w.find(u"accountHeader"_s)));
        QVERIFY(isVisible(w.find(u"accountCard"_s)));
        QTRY_VERIFY(isVisible(w.find(u"linkButton"_s)));
        QVERIFY(!isVisible(w.find(u"unlinkButton"_s)));
        QVERIFY(!isVisible(w.find(u"cancelLinkButton"_s)));
        QVERIFY(isVisible(w.find(u"accountNotes"_s)));
        QVERIFY(!isVisible(w.find(u"accountCharactersRow"_s)));
        QCOMPARE(w.find(u"accountDisclaimer"_s)->property("text").toString(),
                 u"This product isn't affiliated with or endorsed by Grinding Gear Games in any way."_s);
    }

    void listsTheCharactersOfALinkedAccount()
    {
        Window w(true, true);
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.shown());
        QTRY_VERIFY(w.app->poeAccount()->isLinked());
        QTRY_VERIFY(w.app->accountCharacters()->isLoaded());
        QVERIFY(w.shown());
        QVERIFY(isVisible(w.find(u"unlinkButton"_s)));
        QVERIFY(!isVisible(w.find(u"linkButton"_s)));
        QVERIFY(!isVisible(w.find(u"accountNotes"_s)));
        QVERIFY(isVisible(w.find(u"accountCharactersRow"_s)));
        QVERIFY(isVisible(w.find(u"accountDisclaimer"_s)));
        QCOMPARE(findAll(w.page(), u"importAccountButton"_s).size(), 2);
        QObject *one = w.find(u"account:FakeOne"_s);
        QCOMPARE(one->property("text").toString(), u"FakeOne"_s);
        QCOMPARE(one->property("description").toString(), u"Level 92 Stormweaver, Standard"_s);
        QCOMPARE(w.find(u"account:FakeTwo"_s)->property("description").toString(),
                 u"Level 7 Ranger, HC Fake League"_s);
    }

    void importsOneAndThenOffersToUpdateIt()
    {
        Window w(true, true);
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.shown());
        QTRY_VERIFY(w.app->accountCharacters()->isLoaded());
        QVERIFY(w.shown());
        QObject *button = findAll(w.page(), u"importAccountButton"_s).value(0);
        QVERIFY(button);
        QCOMPARE(button->property("text").toString(), u"Import"_s);
        QVERIFY(click(button));
        QTRY_VERIFY(!w.app->accountCharacters()->isImporting());
        QVERIFY(w.shown());
        QCOMPARE(w.app->characterList()->rowCount(), 1);
        QVERIFY(isVisible(w.find(u"accountImportMessage"_s)));
        QVERIFY(w.find(u"accountImportMessage"_s)
                    ->property("text")
                    .toString()
                    .startsWith(u"FakeOne now wears 1 item from the game."_s));
        QCOMPARE(findAll(w.page(), u"importAccountButton"_s).value(0)->property("text").toString(),
                 u"Update From the Game"_s);
        QCOMPARE(findAll(w.page(), u"importAccountButton"_s).value(1)->property("text").toString(),
                 u"Import"_s);
    }

    void saysWhyTheListIsMissing()
    {
        Window w(true, true);
        w.network->route = [](const QNetworkRequest &request) -> std::optional<FakeAnswer> {
            if (request.url().host() == u"api.pathofexile.com"_s)
                return FakeAnswer{.status = 500, .body = "{}"};
            const QByteArray body = R"({"access_token":"fake-access","refresh_token":"fake-refresh-2",)"
                                    R"("expires_in":36000,"token_type":"bearer"})";
            return FakeAnswer{.body = body};
        };
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.shown());
        QTRY_VERIFY(!w.app->accountCharacters()->error().isEmpty());
        QVERIFY(w.shown());
        QVERIFY(isVisible(w.find(u"accountCharactersError"_s)));
        QCOMPARE(findAll(w.page(), u"importAccountButton"_s).size(), 0);
    }

    void unlinkingGoesBackToTheLinkButton()
    {
        Window w(true, true);
        QVERIFY(w.show(u"charactersAction"_s));
        QVERIFY(w.shown());
        QTRY_VERIFY(w.app->accountCharacters()->isLoaded());
        QVERIFY(w.shown());
        QVERIFY(click(w.find(u"unlinkButton"_s)));
        QTRY_VERIFY(!w.app->poeAccount()->isLinked());
        QVERIFY(w.shown());
        QVERIFY(isVisible(w.find(u"linkButton"_s)));
        QVERIFY(!isVisible(w.find(u"accountCharactersRow"_s)));
        QCOMPARE(findAll(w.page(), u"importAccountButton"_s).size(), 0);
    }
};

QTEST_MAIN(TestAccountCard)
#include "tst_accountcard.moc"
