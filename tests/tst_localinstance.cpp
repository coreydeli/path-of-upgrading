// SPDX-License-Identifier: MIT
#include "platform/localinstance.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

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

        LocalInstance second(m_name);
        QVERIFY(!second.claim({u"path-of-upgrading"_s, u"--background"_s}));
        QTRY_COMPARE(started.size(), 1);
        QCOMPARE(started.first().first().toStringList(),
                 (QStringList{u"path-of-upgrading"_s, u"--background"_s}));

        // and again
        LocalInstance third(m_name);
        QVERIFY(!third.claim({u"path-of-upgrading"_s}));
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
        LocalInstance after(m_name);
        QVERIFY(!after.claim({u"path-of-upgrading"_s}));
        QTRY_COMPARE(started.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestLocalInstance)
#include "tst_localinstance.moc"
