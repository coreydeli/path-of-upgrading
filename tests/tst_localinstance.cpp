// SPDX-License-Identifier: MIT
#include "platform/localinstance.h"

#include <QCoreApplication>
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
};

QTEST_GUILESS_MAIN(TestLocalInstance)
#include "tst_localinstance.moc"
