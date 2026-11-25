// SPDX-License-Identifier: MIT
#include "fakenetwork.h"
#include "services/poeclient.h"

#include <QSignalSpy>
#include <QTest>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// made-up
const PoeConfig config{.clientId = u"examplepou"_s, .contact = u"example.invalid"_s, .ports = {1}};

// Gives the tokens it was given in turn, a moment later.
class FakeTokens : public PoeTokenSource
{
public:
    QStringList tokens{u"fake-token-1"_s, u"fake-token-2"_s, u"fake-token-3"_s};
    QString error; // when it can't give one
    int asked = 0;
    int rejected = 0;

    void withToken(const QObject *context, Handler handler) override
    {
        ++asked;
        const QString token = error.isEmpty() ? tokens.value(asked - 1) : QString();
        QTimer::singleShot(0, context, [handler, token, this] { handler(token, error); });
    }
    void rejectToken() override { ++rejected; }
};

const QUrl somewhere(u"https://api.example.invalid/character/poe2"_s);

struct Setup
{
    FakeNetwork network;
    FakeTokens tokens;
    PoeClient client{&network, config, &tokens};
    QObject context;
    QList<PoeResponse> got;

    void ask(const QUrl &url = somewhere)
    {
        client.get(url, &context, [this](const PoeResponse &r) { got << r; });
    }
    bool answered(int count)
    {
        return QTest::qWaitFor([this, count] { return got.size() >= count; }, 2000);
    }
};

} // namespace

class TestPoeClient : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void asksWithTheTokenAndWhoItIs()
    {
        Setup s;
        s.network.answers << FakeAnswer{.body = "{\"characters\":[]}"};
        s.ask();
        QVERIFY(s.answered(1));
        QVERIFY(s.got.first().ok);
        QCOMPARE(s.got.first().body, QByteArray("{\"characters\":[]}"));
        QCOMPARE(s.network.sent.size(), 1);
        const QNetworkRequest &request = s.network.sent.first().request;
        QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer fake-token-1"));
        QVERIFY(request.rawHeader("User-Agent").startsWith("OAuth examplepou/"));
        QVERIFY(request.rawHeader("User-Agent").contains("example.invalid"));
    }

    void asksForTheListAndACharacter()
    {
        Setup s;
        s.client.characters(&s.context, [&s](const PoeResponse &r) { s.got << r; });
        s.client.character(u"Stormcaller"_s, &s.context, [&s](const PoeResponse &r) { s.got << r; });
        QVERIFY(s.answered(2));
        QCOMPARE(s.network.sent.at(0).request.url(), charactersUrl());
        QCOMPARE(s.network.sent.at(1).request.url(), characterUrl(u"Stormcaller"_s));
    }

    void oneAtATimeInOrder()
    {
        Setup s;
        s.network.answers << FakeAnswer{.body = "first"} << FakeAnswer{.body = "second"};
        s.ask(QUrl(u"https://api.example.invalid/one"_s));
        s.ask(QUrl(u"https://api.example.invalid/two"_s));
        QVERIFY(s.client.isBusy());
        // the second waits for the first's answer
        QCOMPARE(s.network.sent.size(), 0);
        QVERIFY(s.answered(2));
        QCOMPARE(s.got.at(0).body, QByteArray("first"));
        QCOMPARE(s.got.at(1).body, QByteArray("second"));
        QCOMPARE(s.network.sent.size(), 2);
        QTRY_VERIFY(!s.client.isBusy());
    }

    void aRefusedTokenIsReplacedOnce()
    {
        Setup s;
        s.network.answers << FakeAnswer{.status = 401} << FakeAnswer{.body = "fine"};
        s.ask();
        QVERIFY(s.answered(1));
        QVERIFY(s.got.first().ok);
        QCOMPARE(s.tokens.rejected, 1);
        QCOMPARE(s.network.sent.size(), 2);
        QCOMPARE(s.network.sent.at(1).request.rawHeader("Authorization"), QByteArray("Bearer fake-token-2"));
    }

    void aSecondRefusalMeansLinkingAgain()
    {
        Setup s;
        s.network.answers << FakeAnswer{.status = 401} << FakeAnswer{.status = 401}
                          << FakeAnswer{.body = "never asked"};
        s.ask();
        QVERIFY(s.answered(1));
        QVERIFY(!s.got.first().ok);
        QCOMPARE(s.got.first().failure.kind, PoeFailure::Kind::SignedOut);
        QCOMPARE(s.network.sent.size(), 2);
        QCOMPARE(s.tokens.rejected, 1);
    }

    void aRefusalToReadIsNotRepeated()
    {
        Setup s;
        s.network.answers << FakeAnswer{.status = 403};
        s.ask();
        QVERIFY(s.answered(1));
        QCOMPARE(s.got.first().failure.kind, PoeFailure::Kind::Refused);
        QCOMPARE(s.network.sent.size(), 1);
        QCOMPARE(s.tokens.rejected, 0);
    }

    void tooManyRequestsSaysHowLongAndHoldsTheNext()
    {
        Setup s;
        s.network.answers << FakeAnswer{.status = 429, .headers = {{"Retry-After", "30"}}}
                          << FakeAnswer{.body = "later"};
        s.ask();
        s.ask();
        QVERIFY(s.answered(1));
        QVERIFY(!s.got.first().ok);
        QCOMPARE(s.got.first().failure.kind, PoeFailure::Kind::RateLimited);
        QCOMPARE(s.got.first().failure.waitSeconds, 30);
        QVERIFY(s.got.first().failure.text.contains(u"30 seconds"_s));
        // nothing is sent again, and the next one is held for the time given
        QTest::qWait(200);
        QCOMPARE(s.network.sent.size(), 1);
        QCOMPARE(s.got.size(), 1);
    }

    void saysWhenTheWebsiteCantBeReached()
    {
        Setup s;
        s.network.answers << FakeAnswer{.error = QNetworkReply::HostNotFoundError};
        s.ask();
        QVERIFY(s.answered(1));
        QVERIFY(!s.got.first().ok);
        QCOMPARE(s.got.first().failure.text,
                 u"Couldn't reach the website. Check your connection and try again."_s);
    }

    void aWebsiteThatIsDown()
    {
        Setup s;
        s.network.answers << FakeAnswer{.status = 503};
        s.ask();
        QVERIFY(s.answered(1));
        QCOMPARE(s.got.first().failure.kind, PoeFailure::Kind::Unavailable);
    }

    void noTokenMeansNoRequest()
    {
        Setup s;
        s.tokens.error = u"Your account isn't linked."_s;
        s.ask();
        QVERIFY(s.answered(1));
        QCOMPARE(s.got.first().failure.kind, PoeFailure::Kind::SignedOut);
        QCOMPARE(s.got.first().failure.text, u"Your account isn't linked."_s);
        QCOMPARE(s.network.sent.size(), 0);
    }

    void aGoneAskerIsNotCalledAndDoesntBlockTheNext()
    {
        Setup s;
        s.network.answers << FakeAnswer{.body = "mine"};
        auto *gone = new QObject;
        bool called = false;
        s.client.get(somewhere, gone, [&called](const PoeResponse &) { called = true; });
        delete gone;
        s.ask();
        QVERIFY(s.answered(1));
        QVERIFY(!called);
        QCOMPARE(s.got.first().body, QByteArray("mine"));
        QCOMPARE(s.network.sent.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestPoeClient)
#include "tst_poeclient.moc"
