// SPDX-License-Identifier: MIT
#include "app/poeaccount.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "fakesecretstore.h"
#include "platform/secretstore.h"
#include "services/poeauth.h"

#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// made-up, with any free port
const PoeConfig config{.clientId = u"examplepou"_s, .contact = u"example.invalid"_s, .ports = {0}};

const QDateTime day0 = QDateTime(QDate(2026, 3, 1), QTime(12, 0), QTimeZone::utc());

QByteArray tokens(const QString &access, const QString &refresh, int expiresIn = 36000)
{
    return QJsonDocument(QJsonObject{{u"access_token"_s, access},
                                     {u"refresh_token"_s, refresh},
                                     {u"expires_in"_s, expiresIn},
                                     {u"token_type"_s, u"bearer"_s}})
        .toJson(QJsonDocument::Compact);
}

QString savedLink(const QString &refresh, const QDateTime &until)
{
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{u"refresh"_s, refresh}, {u"until"_s, until.toString(Qt::ISODate)}})
            .toJson(QJsonDocument::Compact));
}

// The keyring, the website and the opener between them, with a clock to move.
struct Linking
{
    FakeSecretStore store;
    FakeNetwork network;
    FakeOpener opener;
    PoeAuth auth{network, config};
    QDateTime clock = day0;
    PoeAccount account;
    QSignalSpy changed;
    QSignalSpy linkedChanged;

    explicit Linking(bool offered = true)
        : account(offered, &store, &auth, &opener)
        , changed(&account, &PoeAccount::changed)
        , linkedChanged(&account, &PoeAccount::linkedChanged)
    {
        account.setClock([this] { return clock; });
    }

    bool settled() const
    {
        return QTest::qWaitFor([this] { return !account.isBusy(); }, 2000);
    }

    // The browser, going back to the app with this, once the sign in has been
    // opened there. The state is the app's own unless it's given.
    void browserGoesBack(const QString &query, const QString &state = {})
    {
        QVERIFY(!opener.opened.isEmpty());
        const QUrl authorize = opener.opened.last();
        const QUrlQuery asked(authorize);
        const quint16 port = QUrl(asked.queryItemValue(u"redirect_uri"_s, QUrl::FullyDecoded)).port();
        const QString used = state.isEmpty() ? asked.queryItemValue(u"state"_s) : state;
        auto *socket = new QTcpSocket;
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QTcpSocket::connected, socket, [socket, query, used] {
            socket->write("GET /callback?" + query.toUtf8() + "&state=" + used.toUtf8() +
                          " HTTP/1.1\r\n\r\n");
        });
        socket->connectToHost(QHostAddress::LocalHost, port);
    }

    // Links, as a player would, with the website giving these tokens.
    void linkWith(const QString &access, const QString &refresh)
    {
        network.answers << FakeAnswer{.body = tokens(access, refresh)};
        account.link();
        browserGoesBack(u"code=fake-code"_s);
        QTRY_VERIFY(account.isLinked());
        QVERIFY(settled());
    }

    QString kept() const { return store.secrets.value(PoeAccount::secretName()); }
};

QJsonObject keptObject(const Linking &l)
{
    return QJsonDocument::fromJson(l.kept().toUtf8()).object();
}

} // namespace

class TestPoeAccount : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void startsUnlinkedWithoutAskingTheKeyring()
    {
        Linking l;
        QVERIFY(l.account.isOffered());
        QVERIFY(!l.account.isLinked());
        QVERIFY(!l.account.isWaiting());
        QCOMPARE(l.account.status(), u"Your account isn't linked."_s);
        QCOMPARE(l.store.reads, 0);
    }

    void doesNothingWhenNotOffered()
    {
        Linking l(false);
        QVERIFY(!l.account.isOffered());
        l.store.secrets.insert(PoeAccount::secretName(), savedLink(u"fake-refresh"_s, day0.addDays(3)));
        l.account.load();
        l.account.link();
        l.account.unlink();
        QTest::qWait(50);
        QCOMPARE(l.store.reads, 0);
        QCOMPARE(l.store.writes, 0);
        QCOMPARE(l.store.removals, 0);
        QVERIFY(l.opener.opened.isEmpty());
        QVERIFY(!l.account.isLinked());
        // and gives no token
        QString error;
        l.account.withToken(this, [&error](const QString &, const QString &why) { error = why; });
        QTRY_VERIFY(!error.isEmpty());
        QVERIFY(l.network.sent.isEmpty());
    }

    void loadsTheKeptLink()
    {
        Linking l;
        l.store.secrets.insert(PoeAccount::secretName(), savedLink(u"fake-refresh"_s, day0.addDays(3)));
        l.account.load();
        QVERIFY(l.account.isBusy());
        QCOMPARE(l.account.status(), u"Waiting for the keyring."_s);
        QVERIFY(l.settled());
        QVERIFY(l.account.isLinked());
        QCOMPARE(l.linkedChanged.size(), 1);
        QCOMPARE(l.account.status(), u"Your account is linked until %1."_s.arg(QLocale().toString(
                                         day0.addDays(3).toLocalTime().date(), QLocale::LongFormat)));
        // once
        l.account.load();
        QCOMPARE(l.store.reads, 1);
    }

    void loadsNothingKept()
    {
        Linking l;
        l.account.load();
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QCOMPARE(l.account.error(), QString());
    }

    void aLockedKeyringIsSaid()
    {
        Linking l;
        l.store.failure = u"The keyring is locked."_s;
        l.account.load();
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QCOMPARE(l.account.error(),
                 u"Couldn't read your Path of Exile link from the keyring: The keyring is locked."_s);
        QCOMPARE(l.account.status(), u"Couldn't tell whether your account is linked."_s);
        // and it can still be linked once it's unlocked
        l.store.failure.clear();
        l.account.link();
        QVERIFY(l.account.isWaiting());
    }

    void dropsAKeptLinkThatRanOut()
    {
        Linking l;
        l.store.secrets.insert(PoeAccount::secretName(), savedLink(u"fake-refresh"_s, day0.addSecs(-1)));
        l.account.load();
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QVERIFY(l.account.error().startsWith(u"Your link ran out after 7 days"_s));
        QVERIFY(!l.store.secrets.contains(PoeAccount::secretName()));
    }

    void dropsAKeptLinkItCantRead()
    {
        Linking l;
        l.store.secrets.insert(PoeAccount::secretName(), u"not json"_s);
        l.account.load();
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QVERIFY(!l.store.secrets.contains(PoeAccount::secretName()));
        QCOMPARE(l.account.error(), QString());
    }

    void linksThroughTheBrowser()
    {
        Linking l;
        l.network.answers << FakeAnswer{.body = tokens(u"fake-access"_s, u"fake-refresh"_s)};
        l.account.link();
        QVERIFY(l.account.isWaiting());
        QCOMPARE(
            l.account.status(),
            u"Waiting for you in the browser. Sign in to Path of Exile and allow Path of Upgrading there."_s);
        QCOMPARE(l.opener.opened.size(), 1);
        QCOMPARE(l.opener.opened.first().host(), u"www.pathofexile.com"_s);

        l.browserGoesBack(u"code=fake-code"_s);
        QTRY_VERIFY(l.account.isLinked());
        QVERIFY(l.settled());
        QVERIFY(!l.account.isWaiting());
        QCOMPARE(l.account.error(), QString());
        QCOMPARE(l.linkedChanged.size(), 1);

        // the refresh token is kept, with when the link ends, and the access token isn't
        const QJsonObject kept = keptObject(l);
        QCOMPARE(kept.value(u"refresh"_s).toString(), u"fake-refresh"_s);
        QCOMPARE(QDateTime::fromString(kept.value(u"until"_s).toString(), Qt::ISODate), day0.addDays(7));
        QVERIFY(!l.kept().contains(u"fake-access"_s));
    }

    void denied()
    {
        Linking l;
        l.account.link();
        l.browserGoesBack(u"error=access_denied"_s);
        QTRY_VERIFY(!l.account.isWaiting());
        QVERIFY(!l.account.isLinked());
        QCOMPARE(l.account.error(), u"You didn't allow it, so nothing was linked."_s);
        QCOMPARE(l.store.writes, 0);
    }

    void cancelling()
    {
        Linking l;
        l.account.link();
        QVERIFY(l.account.isWaiting());
        l.account.cancel();
        QVERIFY(!l.account.isWaiting());
        QVERIFY(!l.account.isLinked());
        QCOMPARE(l.account.error(), QString());
        QCOMPARE(l.store.writes, 0);
    }

    void cancellingKeepsAnOlderLink()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        l.account.link();
        QVERIFY(l.account.isWaiting());
        QVERIFY(!l.account.isLinked());
        l.account.cancel();
        QVERIFY(l.account.isLinked());
    }

    void aKeyringThatWontKeepItMeansNoLink()
    {
        Linking l;
        l.store.failure = u"The keyring is locked."_s;
        l.network.answers << FakeAnswer{.body = tokens(u"fake-access"_s, u"fake-refresh"_s)};
        l.account.link();
        l.browserGoesBack(u"code=fake-code"_s);
        QTRY_COMPARE(l.store.writes, 1);
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QCOMPARE(l.account.error(), u"Couldn't keep your link in the keyring: The keyring is locked."_s);
        // nothing is kept in memory either
        QString error;
        l.account.withToken(this, [&error](const QString &, const QString &why) { error = why; });
        QTRY_VERIFY(!error.isEmpty());
    }

    void giveTheCachedToken()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        QString token;
        QString error = u"unset"_s;
        l.account.withToken(this, [&](const QString &got, const QString &why) {
            token = got;
            error = why;
        });
        // later, not from inside the call
        QVERIFY(token.isEmpty());
        QTRY_COMPARE(token, u"fake-access"_s);
        QCOMPARE(error, QString());
        // only the sign in's one request
        QCOMPARE(l.network.sent.size(), 1);
    }

    void refreshesWhenTheTokenIsAboutToEnd()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        // the access token lasts 10 hours, so 9 hours 59 minutes later it's too near its end
        l.clock = l.clock.addSecs(10 * 3600 - 30);
        l.network.answers << FakeAnswer{.body = tokens(u"newer-access"_s, u"newer-refresh"_s)};

        QStringList tokensGiven;
        const auto want = [&] {
            l.account.withToken(this, [&](const QString &got, const QString &) { tokensGiven << got; });
        };
        // two asking at once share the one refresh
        want();
        want();
        QTRY_COMPARE(tokensGiven.size(), 2);
        QCOMPARE(tokensGiven, (QStringList{u"newer-access"_s, u"newer-access"_s}));
        QCOMPARE(l.network.sent.size(), 2);
        const QUrlQuery body(QString::fromUtf8(l.network.sent.last().body));
        QCOMPARE(body.queryItemValue(u"grant_type"_s), u"refresh_token"_s);
        QCOMPARE(body.queryItemValue(u"refresh_token"_s), u"fake-refresh"_s);

        // the new refresh token is kept, and the link still ends when it did
        QVERIFY(l.settled());
        const QJsonObject kept = keptObject(l);
        QCOMPARE(kept.value(u"refresh"_s).toString(), u"newer-refresh"_s);
        QCOMPARE(QDateTime::fromString(kept.value(u"until"_s).toString(), Qt::ISODate), day0.addDays(7));
        QVERIFY(l.account.isLinked());
    }

    void aRefreshAfterALoadWorks()
    {
        Linking l;
        l.store.secrets.insert(PoeAccount::secretName(), savedLink(u"fake-refresh"_s, day0.addDays(3)));
        l.account.load();
        QVERIFY(l.settled());
        l.network.answers << FakeAnswer{.body = tokens(u"new-access"_s, u"new-refresh"_s)};
        QString token;
        l.account.withToken(this, [&](const QString &got, const QString &) { token = got; });
        QTRY_COMPARE(token, u"new-access"_s);
    }

    void aRefusedRefreshMeansLinkingAgain()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        l.clock = l.clock.addSecs(11 * 3600);
        l.network.answers << FakeAnswer{.status = 400, .body = R"({"error":"invalid_grant"})"};
        QString error;
        l.account.withToken(this, [&](const QString &, const QString &why) { error = why; });
        QTRY_COMPARE(error, u"Your link ran out. Link your account again."_s);
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QCOMPARE(l.account.error(), u"Your link ran out. Link your account again."_s);
        QVERIFY(!l.store.secrets.contains(PoeAccount::secretName()));
    }

    void aBusyWebsiteKeepsTheLink()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        l.clock = l.clock.addSecs(11 * 3600);
        l.network.answers << FakeAnswer{.status = 429, .body = "{}", .headers = {{"Retry-After", "30"}}};
        QString error;
        l.account.withToken(this, [&](const QString &, const QString &why) { error = why; });
        QTRY_COMPARE(error, u"The website says to wait 30 seconds before asking again."_s);
        QVERIFY(l.account.isLinked());
        QVERIFY(l.store.secrets.contains(PoeAccount::secretName()));
        // one request, and no retry
        QCOMPARE(l.network.sent.size(), 2);
    }

    void aLinkPast7DaysIsDroppedWithoutAsking()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        l.clock = l.clock.addDays(7).addSecs(1);
        QString error;
        l.account.withToken(this, [&](const QString &, const QString &why) { error = why; });
        QTRY_VERIFY(error.startsWith(u"Your link ran out after 7 days"_s));
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QVERIFY(!l.store.secrets.contains(PoeAccount::secretName()));
        // the website wasn't asked
        QCOMPARE(l.network.sent.size(), 1);
    }

    void noTokenWhenNotLinked()
    {
        Linking l;
        QString error;
        l.account.withToken(this, [&](const QString &, const QString &why) { error = why; });
        QTRY_COMPARE(error, u"Your account isn't linked."_s);
        QVERIFY(l.network.sent.isEmpty());
    }

    void aGoneContextIsNotCalled()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        l.clock = l.clock.addSecs(11 * 3600);
        l.network.answers << FakeAnswer{.body = tokens(u"newer-access"_s, u"newer-refresh"_s)};
        bool called = false;
        {
            QObject gone;
            l.account.withToken(&gone, [&called](const QString &, const QString &) { called = true; });
        }
        QVERIFY(l.settled());
        QTRY_COMPARE(l.network.sent.size(), 2);
        QTest::qWait(50);
        QVERIFY(!called);
    }

    void unlinks()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        l.account.unlink();
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QCOMPARE(l.linkedChanged.size(), 2);
        QVERIFY(!l.store.secrets.contains(PoeAccount::secretName()));
        QCOMPARE(l.account.error(), QString());
        // and the token's gone
        QString error;
        l.account.withToken(this, [&](const QString &, const QString &why) { error = why; });
        QTRY_COMPARE(error, u"Your account isn't linked."_s);
    }

    void unlinkingSaysWhenTheKeyringWontLetGo()
    {
        Linking l;
        l.linkWith(u"fake-access"_s, u"fake-refresh"_s);
        l.store.failure = u"The keyring is locked."_s;
        l.account.unlink();
        QVERIFY(l.settled());
        QVERIFY(!l.account.isLinked());
        QCOMPARE(l.account.error(),
                 u"Couldn't remove your Path of Exile link from the keyring: The keyring is locked."_s);
    }

    void unlinkingStopsAWait()
    {
        Linking l;
        l.account.link();
        l.account.unlink();
        QVERIFY(!l.account.isWaiting());
    }

    void opensTheWebsitesApplicationsPage()
    {
        Linking l;
        l.account.openRevokePage();
        QCOMPARE(l.opener.opened,
                 (QList<QUrl>{QUrl(u"https://www.pathofexile.com/my-account/applications"_s)}));
    }

    void saysWhatTheWebsiteAsksFor()
    {
        Linking l;
        QCOMPARE(l.account.disclaimer(),
                 u"This product isn't affiliated with or endorsed by Grinding Gear Games in any way."_s);
        QVERIFY(!l.account.notes().isEmpty());
        for (const QString &note : l.account.notes()) {
            QVERIFY(!note.contains(u'—'));
            QVERIFY(!note.contains(u'–'));
        }
    }
};

QTEST_GUILESS_MAIN(TestPoeAccount)
#include "tst_poeaccount.moc"
