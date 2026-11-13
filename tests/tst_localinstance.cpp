// SPDX-License-Identifier: MIT
#include "platform/localinstance.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <memory>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// Started again, as another program would be, while the first one reads what
// it's handed. On Windows it can't hand it over any other way, as the pipe
// holds nothing until the first one reads it.
bool claimElsewhere(const QString &name, const QStringList &arguments)
{
    bool claimed = true;
    const std::unique_ptr<QThread> thread(
        QThread::create([&] { claimed = LocalInstance(name).claim(arguments); }));
    thread->start();
    while (!thread->wait(10))
        QCoreApplication::processEvents();
    return claimed;
}

// Sends these bytes, as some other program could, and says whether the first
// one hung up on it within the wait.
bool hungUpOn(const QString &name, const QByteArray &bytes, int wait)
{
    bool hungUp = false;
    const std::unique_ptr<QThread> thread(QThread::create([&] {
        QLocalSocket socket;
        socket.connectToServer(name);
        if (!socket.waitForConnected(1000))
            return;
        socket.write(bytes);
        socket.waitForBytesWritten(1000);
        hungUp = socket.state() == QLocalSocket::UnconnectedState || socket.waitForDisconnected(wait);
    }));
    thread->start();
    while (!thread->wait(10))
        QCoreApplication::processEvents();
    return hungUp;
}

// What a list of this many arguments starts with, and the first of them,
// which says it's this many bytes long.
QByteArray listHead(quint32 count, quint32 firstBytes)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_8);
    stream << count << firstBytes;
    return bytes;
}

} // namespace

class TestLocalInstance : public QObject
{
    Q_OBJECT

private:
    // one of its own for each test, which no other copy of the test has
    QString m_name;

private Q_SLOTS:
    // as when the first one couldn't be reached
    void initTestCase() { QTest::failOnWarning(); }

    void init()
    {
        static int count = 0;
        m_name = u"tst_localinstance-%1-%2"_s.arg(QCoreApplication::applicationPid()).arg(++count);
    }

    void firstClaims()
    {
        LocalInstance first(m_name);
        QVERIFY(first.claim({u"path-of-upgrading"_s}));
    }

    void secondHandsOver()
    {
        LocalInstance first(m_name);
        QVERIFY(first.claim({u"path-of-upgrading"_s}));
        QSignalSpy started(&first, &LocalInstance::startedAgain);

        QVERIFY(!claimElsewhere(m_name, {u"path-of-upgrading"_s, u"--background"_s}));
        QTRY_COMPARE(started.size(), 1);
        QCOMPARE(started.first().first().toStringList(),
                 (QStringList{u"path-of-upgrading"_s, u"--background"_s}));

        // and again
        QVERIFY(!claimElsewhere(m_name, {u"path-of-upgrading"_s}));
        QTRY_COMPARE(started.size(), 2);
        QCOMPARE(started.last().first().toStringList(), QStringList{u"path-of-upgrading"_s});
    }

    void claimedOnceTheFirstHasGone()
    {
        {
            LocalInstance first(m_name);
            QVERIFY(first.claim({u"path-of-upgrading"_s}));
        }
        LocalInstance next(m_name);
        QVERIFY(next.claim({u"path-of-upgrading"_s}));
        QSignalSpy started(&next, &LocalInstance::startedAgain);
        QVERIFY(!claimElsewhere(m_name, {u"path-of-upgrading"_s}));
        QTRY_COMPARE(started.size(), 1);
    }

    void turnsAwayTooManyArguments()
    {
        LocalInstance first(m_name);
        QVERIFY(first.claim({u"path-of-upgrading"_s}));
        QSignalSpy started(&first, &LocalInstance::startedAgain);
        // which, read as a list, would be reserved at once
        QTest::ignoreMessage(QtWarningMsg,
                             "Something that isn't a copy of the program sent too much, and was turned away");
        QVERIFY(hungUpOn(m_name, listHead(0xfffffff0, 0), 2000));
        QVERIFY(started.isEmpty());
        // and a copy is still heard
        QVERIFY(!claimElsewhere(m_name, {u"path-of-upgrading"_s}));
        QTRY_COMPARE(started.size(), 1);
    }

    void turnsAwayTooMuch()
    {
        LocalInstance first(m_name);
        QVERIFY(first.claim({u"path-of-upgrading"_s}));
        QSignalSpy started(&first, &LocalInstance::startedAgain);
        QTest::ignoreMessage(QtWarningMsg,
                             "Something that isn't a copy of the program sent too much, and was turned away");
        // an argument a gigabyte long, of which it gets the first 100 KB
        QVERIFY(hungUpOn(m_name, listHead(1, 1 << 30) + QByteArray(100 * 1024, 'a'), 2000));
        QVERIFY(started.isEmpty());
    }

    void hangsUpOnOneThatStalls()
    {
        LocalInstance first(m_name);
        first.setReadTime(std::chrono::milliseconds(200));
        QVERIFY(first.claim({u"path-of-upgrading"_s}));
        QSignalSpy started(&first, &LocalInstance::startedAgain);
        // two arguments, and the first never comes
        QVERIFY(hungUpOn(m_name, listHead(2, 10), 3000));
        QVERIFY(started.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLocalInstance)
#include "tst_localinstance.moc"
