// SPDX-License-Identifier: MIT
#include "core/sessionguide.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestSessionGuide : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stepsAreNumbered()
    {
        const SessionGuide guide = guideToSessionId();
        QVERIFY(guide.steps.size() > 1);
        for (qsizetype i = 0; i < guide.steps.size(); ++i)
            QVERIFY2(guide.steps[i].startsWith(QString::number(i + 1) + u". "_s), qPrintable(guide.steps[i]));
    }

    void startsByLoggingIn() { QVERIFY(guideToSessionId().steps.first().contains(u"Sign in"_s)); }

    void namesTheCookieInBothKindsOfBrowser()
    {
        const QString steps = guideToSessionId().steps.join(u'\n');
        QVERIFY(steps.contains(u"F12"_s));
        QVERIFY(steps.contains(u"Firefox"_s));
        QVERIFY(steps.contains(u"Storage"_s));
        QVERIFY(steps.contains(u"Chromium"_s));
        QVERIFY(steps.contains(u"Application"_s));
        QVERIFY(steps.contains(u"Cookies"_s));
        QVERIFY(steps.contains(u"https://www.pathofexile.com"_s));
        QVERIFY(steps.contains(u"POESESSID"_s));
    }

    void warnsAndSaysWhereItIsKept()
    {
        const QString notes = guideToSessionId().notes.join(u' ');
        QVERIFY(notes.contains(u"Keep it to yourself"_s));
        QVERIFY(notes.contains(u"system keyring"_s));
        QVERIFY(notes.contains(u"Signing out"_s));
    }

    // the id is a cookie, so no address has one in it
    void signInIsAPlainPage()
    {
        const QUrl url = signInUrl();
        QCOMPARE(url.scheme(), u"https"_s);
        QCOMPARE(url.host(), u"www.pathofexile.com"_s);
        QVERIFY(!url.hasQuery());
        QVERIFY(!url.hasFragment());
    }
};

QTEST_GUILESS_MAIN(TestSessionGuide)
#include "tst_sessionguide.moc"
