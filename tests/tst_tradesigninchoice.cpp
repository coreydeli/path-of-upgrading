// SPDX-License-Identifier: MIT
#include "app/poeaccount.h"
#include "app/settingsstore.h"
#include "app/tradesession.h"
#include "app/tradesigninchoice.h"
#include "core/poeoauth.h"
#include "fakenetwork.h"
#include "fakesecretstore.h"
#include "services/poeauth.h"
#include "services/tradeclient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// made-up
const PoeConfig config{.clientId = u"examplepou"_s, .contact = u"example.invalid"_s, .ports = {0}};
const QString fakeId = u"0123456789abcdef0123456789abcdef"_s;
const QUrl apiBase(u"https://api.example.invalid/trade"_s);

QString savedLink()
{
    const QDateTime until = QDateTime::currentDateTimeUtc().addDays(3);
    return QString::fromUtf8(QJsonDocument(QJsonObject{{u"refresh"_s, u"fake-refresh"_s},
                                                       {u"until"_s, until.toString(Qt::ISODate)}})
                                 .toJson(QJsonDocument::Compact));
}

QByteArray newTokens()
{
    return QJsonDocument(QJsonObject{{u"access_token"_s, u"fake-access"_s},
                                     {u"refresh_token"_s, u"fake-refresh-2"_s},
                                     {u"expires_in"_s, 36000},
                                     {u"token_type"_s, u"bearer"_s}})
        .toJson(QJsonDocument::Compact);
}

// The keyring, the network and everything that signs in, between them.
struct Choosing
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    FakeSecretStore store;
    FakeNetwork network;
    TradeClient client{&network};
    TradeSession session{&store, &client};
    PoeAuth auth{network, config};
    PoeAccount account;
    TradeSignInChoice choice;
    QSignalSpy changed{&choice, &TradeSignInChoice::changed};

    explicit Choosing(bool offered, const QUrl &base = apiBase)
        : account(offered, &store, &auth)
        , choice(&settings, &client, &session, &account, base)
    {}

    bool settled() const
    {
        return QTest::qWaitFor([this] { return !session.isBusy() && !account.isBusy(); }, 1000);
    }

    void saveBoth()
    {
        store.secrets.insert(TradeSession::secretName(), fakeId);
        store.secrets.insert(PoeAccount::secretName(), savedLink());
        session.load();
        account.load();
        QVERIFY(settled());
    }

    // sends a request, and gives back what went out
    QNetworkRequest sendOne()
    {
        const qsizetype before = network.sent.size();
        TradeReply *reply = client.data(u"stats"_s);
        QSignalSpy finished(reply, &TradeReply::finished);
        if (!reply->isFinished())
            finished.wait(1000);
        delete reply;
        return network.sent.size() > before ? network.sent.last().request : QNetworkRequest();
    }
};

} // namespace

class TestTradeSignInChoice : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void theSessionIdIsTheDefault()
    {
        Choosing c(true);
        QCOMPARE(c.choice.kind(), TradeSignInKind::Session);
        QVERIFY(!c.choice.isSignedIn());
        c.saveBoth();
        QVERIFY(c.choice.isSignedIn());
        QCOMPARE(c.sendOne().rawHeader("Cookie"), "POESESSID=" + fakeId.toLatin1());
    }

    void theAccountSignsRequestsWithItsToken()
    {
        Choosing c(true);
        c.saveBoth();
        // the website gives a new access token for the saved link
        c.network.answers << FakeAnswer{.body = newTokens()};
        c.choice.choose(TradeSignInKind::Account);
        QCOMPARE(c.choice.kind(), TradeSignInKind::Account);
        QVERIFY(c.choice.isSignedIn());
        const QNetworkRequest sent = c.sendOne();
        QCOMPARE(sent.url(), QUrl(u"https://api.example.invalid/trade/data/stats"_s));
        QCOMPARE(sent.rawHeader("Authorization"), "Bearer fake-access");
        // and never the cookie, which is still saved
        QVERIFY(!sent.hasRawHeader("Cookie"));
        QVERIFY(c.session.isSaved());
    }

    void savingAnIdWhileTheAccountIsChosenKeepsTheAccount()
    {
        Choosing c(true);
        c.store.secrets.insert(PoeAccount::secretName(), savedLink());
        c.account.load();
        QVERIFY(c.settled());
        c.choice.choose(TradeSignInKind::Account);
        c.session.save(fakeId);
        QVERIFY(c.settled());
        c.network.answers << FakeAnswer{.body = newTokens()};
        const QNetworkRequest sent = c.sendOne();
        QVERIFY(!sent.hasRawHeader("Cookie"));
        QCOMPARE(sent.rawHeader("Authorization"), "Bearer fake-access");
    }

    void choosingTheSessionAgainPutsItsIdBack()
    {
        Choosing c(true);
        c.saveBoth();
        c.choice.choose(TradeSignInKind::Account);
        c.choice.choose(TradeSignInKind::Session);
        QCOMPARE(c.choice.kind(), TradeSignInKind::Session);
        const QNetworkRequest sent = c.sendOne();
        QCOMPARE(sent.rawHeader("Cookie"), "POESESSID=" + fakeId.toLatin1());
        QVERIFY(!sent.hasRawHeader("Authorization"));
    }

    void signedInMeansTheChosenOne()
    {
        Choosing c(true);
        // an account linked, and no session id
        c.store.secrets.insert(PoeAccount::secretName(), savedLink());
        c.account.load();
        QVERIFY(c.settled());
        QVERIFY(!c.choice.isSignedIn());
        c.choice.choose(TradeSignInKind::Account);
        QVERIFY(c.choice.isSignedIn());
        // and the other way round
        Choosing d(true);
        d.store.secrets.insert(TradeSession::secretName(), fakeId);
        d.session.load();
        QVERIFY(d.settled());
        QVERIFY(d.choice.isSignedIn());
        d.choice.choose(TradeSignInKind::Account);
        QVERIFY(!d.choice.isSignedIn());
    }

    void saysSoWhenTheLinkOrTheIdChanges()
    {
        Choosing c(true);
        c.changed.clear();
        c.store.secrets.insert(TradeSession::secretName(), fakeId);
        c.session.load();
        QVERIFY(c.settled());
        QCOMPARE(c.changed.size(), 1);
        c.store.secrets.insert(PoeAccount::secretName(), savedLink());
        c.account.load();
        QVERIFY(c.settled());
        QVERIFY(c.changed.size() >= 2);
        const int before = c.changed.size();
        c.choice.choose(TradeSignInKind::Account);
        QCOMPARE(c.changed.size(), before + 1);
        // the same again changes nothing
        c.choice.choose(TradeSignInKind::Account);
        QCOMPARE(c.changed.size(), before + 1);
    }

    void theChoiceIsSavedAndThenFollowed()
    {
        Choosing c(true);
        QVERIFY(c.choice.isOffered());
        QVERIFY(!c.choice.useAccount());
        c.choice.setUseAccount(true);
        QCOMPARE(c.settings.settings().trade.signIn, TradeSignInKind::Account);
        // the app follows the settings, as this does
        c.choice.choose(c.settings.settings().trade.signIn);
        QVERIFY(c.choice.useAccount());
        c.choice.setUseAccount(false);
        QCOMPARE(c.settings.settings().trade.signIn, TradeSignInKind::Session);
    }

    void theStatusSaysWhichIsInUseAndWhetherItIsReady()
    {
        Choosing c(true);
        QCOMPARE(c.choice.status(), u"Searches are made with your session ID."_s);
        c.choice.choose(TradeSignInKind::Account);
        QCOMPARE(c.choice.status(),
                 u"Your Path of Exile account isn't linked yet. Link it on the Characters page."_s);
        c.store.secrets.insert(PoeAccount::secretName(), savedLink());
        c.account.load();
        QVERIFY(c.settled());
        QCOMPARE(c.choice.status(), u"Searches are made as your linked Path of Exile account."_s);
    }

    void aBuildWithoutTheAccountSavesNothing()
    {
        Choosing c(false);
        c.choice.setUseAccount(true);
        QCOMPARE(c.settings.settings().trade.signIn, TradeSignInKind::Session);
        QVERIFY(!c.choice.useAccount());
    }

    void withoutAnAccountOrAnAddressItStaysOnTheSession()
    {
        // an account that isn't offered, as in a release
        Choosing release(false);
        QVERIFY(!release.choice.isOffered());
        release.choice.choose(TradeSignInKind::Account);
        QCOMPARE(release.choice.kind(), TradeSignInKind::Session);
        // one that is, in a build with no address for trade
        Choosing none(true, {});
        QVERIFY(!none.choice.isOffered());
        none.choice.choose(TradeSignInKind::Account);
        QCOMPARE(none.choice.kind(), TradeSignInKind::Session);
        none.saveBoth();
        QCOMPARE(none.sendOne().rawHeader("Cookie"), "POESESSID=" + fakeId.toLatin1());
        QVERIFY(!none.sendOne().hasRawHeader("Authorization"));
    }

    void anAccountThatIsntLinkedSendsNothing()
    {
        Choosing c(true);
        c.choice.choose(TradeSignInKind::Account);
        QVERIFY(!c.choice.isSignedIn());
        TradeReply *reply = c.client.data(u"stats"_s);
        QSignalSpy finished(reply, &TradeReply::finished);
        QVERIFY(reply->isFinished() || finished.wait(1000));
        QCOMPARE(reply->error(), TradeReply::Error::Rejected);
        QCOMPARE(c.network.sent.size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestTradeSignInChoice)
#include "tst_tradesigninchoice.moc"
