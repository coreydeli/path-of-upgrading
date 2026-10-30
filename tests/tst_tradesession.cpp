// SPDX-License-Identifier: MIT
#include "app/tradesession.h"
#include "fakenetwork.h"
#include "fakesecretstore.h"
#include "services/tradeclient.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// Made up, and not a real one
const QString fakeId = u"0123456789abcdef0123456789abcdef"_s;
const QString otherId = u"fedcba9876543210fedcba9876543210"_s;
const QString secretName = u"POESESSID"_s;

// The keyring, the trade client and the session between them.
struct Signing
{
    FakeSecretStore store;
    FakeNetwork network;
    TradeClient client{&network};
    TradeSession session{&store, &client};
    QSignalSpy idChanged{&session, &TradeSession::idChanged};
    QSignalSpy stored{&session, &TradeSession::stored};

    // once the keyring has answered
    bool settled() const
    {
        return QTest::qWaitFor([this] { return !session.isBusy(); }, 1000);
    }

    // the cookie the trade site gets with a request, which is empty when
    // it's signed out
    QByteArray cookie()
    {
        delete client.data(u"stats"_s);
        return network.sent.last().request.rawHeader("Cookie");
    }
};

} // namespace

class TestTradeSession : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void notAskedYet()
    {
        Signing s;
        QVERIFY(!s.session.isSaved());
        QVERIFY(!s.session.isBusy());
        QCOMPARE(s.session.status(), u"The keyring is only asked while trade searches are on."_s);
        QCOMPARE(s.session.error(), QString());
        QCOMPARE(s.store.reads, 0);
    }

    void signsInWithTheSavedOne()
    {
        Signing s;
        s.store.secrets.insert(secretName, fakeId);
        QSignalSpy changed(&s.session, &TradeSession::changed);
        s.session.load();
        QVERIFY(s.session.isBusy());
        QCOMPARE(s.session.status(), u"Waiting for the keyring."_s);
        QVERIFY(s.settled());
        QVERIFY(s.session.isSaved());
        QCOMPARE(s.session.status(), u"A session ID is saved."_s);
        QCOMPARE(s.session.error(), QString());
        QCOMPARE(s.cookie(), "POESESSID=" + fakeId.toLatin1());
        QCOMPARE(s.idChanged.size(), 1);
        QCOMPARE(changed.size(), 2);
        // and it stays in the keyring
        QCOMPARE(s.store.secrets.value(secretName), fakeId);
    }

    void noneSaved()
    {
        Signing s;
        s.session.load();
        QVERIFY(s.settled());
        QVERIFY(!s.session.isSaved());
        QCOMPARE(s.session.status(), u"No session ID is saved."_s);
        QCOMPARE(s.session.error(), QString());
        QCOMPARE(s.cookie(), QByteArray());
        QCOMPARE(s.idChanged.size(), 0);
    }

    void askedOnce()
    {
        Signing s;
        s.session.load();
        // while it waits, and after
        s.session.load();
        QVERIFY(s.settled());
        s.session.load();
        QCOMPARE(s.store.reads, 1);
    }

    void notOneAnyMore()
    {
        // as the keyring has something else under that name
        Signing s;
        s.store.secrets.insert(secretName, u"not a session id!"_s);
        s.session.load();
        QVERIFY(s.settled());
        QVERIFY(!s.session.isSaved());
        QCOMPARE(s.session.error(), QString());
        QCOMPARE(s.cookie(), QByteArray());
    }

    void spaceAroundTheSavedOne()
    {
        Signing s;
        s.store.secrets.insert(secretName, u" "_s + fakeId + u"\n"_s);
        s.session.load();
        QVERIFY(s.settled());
        QVERIFY(s.session.isSaved());
        QCOMPARE(s.cookie(), "POESESSID=" + fakeId.toLatin1());
        QCOMPARE(s.idChanged.size(), 1);

        // it's the same one without the space
        s.session.save(fakeId);
        QVERIFY(s.settled());
        QCOMPARE(s.idChanged.size(), 1);
    }

    void keyringFailsToRead()
    {
        Signing s;
        s.store.failure = u"The keyring is locked."_s;
        s.session.load();
        QVERIFY(s.settled());
        QVERIFY(!s.session.isSaved());
        QCOMPARE(s.session.status(), u"Couldn't tell if one is saved."_s);
        QCOMPARE(s.session.error(), u"The keyring is locked."_s);
        QCOMPARE(s.cookie(), QByteArray());
        // and isn't asked again
        s.session.load();
        QCOMPARE(s.store.reads, 1);
        QVERIFY(!s.session.isBusy());
    }

    void save()
    {
        Signing s;
        s.session.load();
        QVERIFY(s.settled());
        s.session.save(u"  "_s + fakeId + u" \n"_s);
        QVERIFY(s.session.isBusy());
        QVERIFY(s.settled());
        QVERIFY(s.session.isSaved());
        QCOMPARE(s.session.error(), QString());
        QCOMPARE(s.store.secrets.value(secretName), fakeId);
        QCOMPARE(s.cookie(), "POESESSID=" + fakeId.toLatin1());
        QCOMPARE(s.stored.size(), 1);
        QCOMPARE(s.idChanged.size(), 1);

        // and another in its place
        s.session.save(otherId);
        QVERIFY(s.settled());
        QCOMPARE(s.store.secrets.value(secretName), otherId);
        QCOMPARE(s.cookie(), "POESESSID=" + otherId.toLatin1());
        QCOMPARE(s.stored.size(), 2);
        QCOMPARE(s.idChanged.size(), 2);
    }

    void saveWithoutAsking()
    {
        // while trade searches are off, as it's you who asks
        Signing s;
        s.session.save(fakeId);
        QVERIFY(s.settled());
        QVERIFY(s.session.isSaved());
        QCOMPARE(s.store.reads, 0);
        // so it isn't read over it later
        s.session.load();
        QCOMPARE(s.store.reads, 0);
        QCOMPARE(s.cookie(), "POESESSID=" + fakeId.toLatin1());
    }

    void sameOneAgain()
    {
        Signing s;
        s.session.save(fakeId);
        QVERIFY(s.settled());
        s.session.save(fakeId);
        QVERIFY(s.settled());
        QCOMPARE(s.stored.size(), 2);
        QCOMPARE(s.idChanged.size(), 1);
    }

    void notASessionId_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << QString();
        QTest::newRow("blank") << u"   "_s;
        QTest::newRow("the cookie's name too") << u"POESESSID="_s + fakeId;
        QTest::newRow("spaces in it") << fakeId.left(8) + u" "_s + fakeId.mid(8);
        QTest::newRow("not ASCII") << fakeId.left(8) + QString(QChar(0x00e9));
    }

    void notASessionId()
    {
        QFETCH(QString, text);
        Signing s;
        QSignalSpy changed(&s.session, &TradeSession::changed);
        s.session.save(text);
        QVERIFY(!s.session.isBusy());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(s.session.error(), u"That isn't a session ID. It's only letters and digits."_s);
        QCOMPARE(s.store.writes, 0);
        QVERIFY(!s.session.isSaved());
        QCOMPARE(s.stored.size(), 0);

        // which goes with the next that's right
        s.session.save(fakeId);
        QCOMPARE(s.session.error(), QString());
        QVERIFY(s.settled());
        QVERIFY(s.session.isSaved());
    }

    void keyringFailsToSave()
    {
        Signing s;
        s.session.save(fakeId);
        QVERIFY(s.settled());
        s.store.failure = u"The keyring is locked."_s;
        s.session.save(otherId);
        QVERIFY(s.settled());
        QCOMPARE(s.session.error(), u"The keyring is locked."_s);
        // so it's still signed in with the one it had
        QVERIFY(s.session.isSaved());
        QCOMPARE(s.cookie(), "POESESSID=" + fakeId.toLatin1());
        QCOMPARE(s.stored.size(), 1);
        QCOMPARE(s.idChanged.size(), 1);

        // and nothing is wrong once it works
        s.store.failure.clear();
        s.session.save(otherId);
        QVERIFY(s.settled());
        QCOMPARE(s.session.error(), QString());
        QCOMPARE(s.cookie(), "POESESSID=" + otherId.toLatin1());
    }

    void saveAfterItFailedToRead()
    {
        Signing s;
        s.store.failure = u"The keyring is locked."_s;
        s.session.load();
        QVERIFY(s.settled());
        s.store.failure.clear();
        s.session.save(fakeId);
        QVERIFY(s.settled());
        QVERIFY(s.session.isSaved());
        QCOMPARE(s.session.error(), QString());
        QCOMPARE(s.session.status(), u"A session ID is saved."_s);
    }

    void forget()
    {
        Signing s;
        s.store.secrets.insert(secretName, fakeId);
        s.session.load();
        QVERIFY(s.settled());
        QCOMPARE(s.cookie(), "POESESSID=" + fakeId.toLatin1());
        s.session.forget();
        QVERIFY(s.session.isBusy());
        QVERIFY(s.settled());
        QVERIFY(!s.session.isSaved());
        QCOMPARE(s.session.status(), u"No session ID is saved."_s);
        QCOMPARE(s.session.error(), QString());
        QVERIFY(!s.store.secrets.contains(secretName));
        QCOMPARE(s.cookie(), QByteArray());
        QCOMPARE(s.idChanged.size(), 2);
    }

    void forgetNone()
    {
        Signing s;
        s.session.forget();
        QVERIFY(s.settled());
        QVERIFY(!s.session.isSaved());
        QCOMPARE(s.session.error(), QString());
        QCOMPARE(s.store.removals, 1);
        QCOMPARE(s.idChanged.size(), 0);
    }

    void forgetWhatIsGone()
    {
        // as when it was taken out of the keyring some other way
        Signing s;
        s.session.save(fakeId);
        QVERIFY(s.settled());
        s.store.secrets.clear();
        s.session.forget();
        QVERIFY(s.settled());
        QCOMPARE(s.session.error(), QString());
        QVERIFY(!s.session.isSaved());
        QCOMPARE(s.cookie(), QByteArray());
        QCOMPARE(s.idChanged.size(), 2);
    }

    void keyringFailsToForget()
    {
        Signing s;
        s.session.save(fakeId);
        QVERIFY(s.settled());
        s.store.failure = u"The keyring is locked."_s;
        s.session.forget();
        QVERIFY(s.settled());
        QCOMPARE(s.session.error(), u"The keyring is locked."_s);
        QVERIFY(s.session.isSaved());
        QCOMPARE(s.cookie(), "POESESSID=" + fakeId.toLatin1());
        QCOMPARE(s.idChanged.size(), 1);
    }

    void oneThingAtATime()
    {
        Signing s;
        s.session.save(fakeId);
        s.session.save(otherId);
        s.session.forget();
        QVERIFY(s.settled());
        QCOMPARE(s.store.writes, 1);
        QCOMPARE(s.store.removals, 0);
        QCOMPARE(s.store.secrets.value(secretName), fakeId);
    }

    void goneBeforeItAnswers()
    {
        // which the keyring then has nowhere to tell
        FakeSecretStore store;
        FakeNetwork network;
        TradeClient client(&network);
        {
            TradeSession session(&store, &client);
            session.save(fakeId);
        }
        QTest::qWait(20);
        QCOMPARE(store.writes, 1);
        delete client.data(u"stats"_s);
        QCOMPARE(network.sent.last().request.rawHeader("Cookie"), QByteArray());
    }
};

QTEST_GUILESS_MAIN(TestTradeSession)
#include "tst_tradesession.moc"
