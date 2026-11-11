// SPDX-License-Identifier: MIT
#include "app/settingsstore.h"
#include "app/updates.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "services/updatechecker.h"
#include "updatetexts.h"

#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Version v(const QString &text)
{
    return parseVersion(text).value_or(Version{});
}

FakeAnswer releases(const QStringList &versions)
{
    QList<QJsonObject> list;
    for (const QString &version : versions)
        list << fakeRelease(version, version.contains(u"beta"_s));
    return {.body = fakeReleases(list)};
}

// The model on a checker that has the version 0.6.0, and follows the settings
// as the app does.
struct Setup
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    FakeNetwork network;
    UpdateChecker checker{&network, v(u"0.6.0"_s)};
    FakeOpener opener;
    Updates updates{&checker, &settings, &opener};

    Setup()
    {
        QObject::connect(&settings, &SettingsStore::changed, &settings,
                         [this] { checker.follow(settings.settings().updates); });
        checker.follow(settings.settings().updates);
    }

    void look(const QStringList &versions)
    {
        network.answers = {releases(versions)};
        updates.checkNow();
        QVERIFY(QTest::qWaitFor([&] { return !updates.isChecking(); }, 5000));
    }
};

} // namespace

class TestUpdatesModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLoggingCategory::setFilterRules(u"pathofupgrading.updates.warning=false"_s); }

    void sayNothingBeforeTheFirstLook()
    {
        Setup s;
        QCOMPARE(s.updates.version(), u"0.6.0"_s);
        QVERIFY(s.updates.status().isEmpty());
        QVERIFY(!s.updates.isPending());
        QVERIFY(s.updates.newVersion().isEmpty());
        QVERIFY(s.updates.notes().isEmpty());
    }

    void offersANewerRelease()
    {
        Setup s;
        QSignalSpy offered(&s.updates, &Updates::offered);
        s.network.answers = {releases({u"0.7.0"_s})};
        s.updates.checkNow();
        QCOMPARE(s.updates.status(), u"Looking for updates."_s);
        QVERIFY(s.updates.isChecking());
        QVERIFY(QTest::qWaitFor([&] { return !s.updates.isChecking(); }, 5000));
        QCOMPARE(s.updates.status(), u"Version 0.7.0 is out."_s);
        QVERIFY(s.updates.isPending());
        QCOMPARE(s.updates.newVersion(), u"0.7.0"_s);
        QCOMPARE(s.updates.notes(), u"What is new in 0.7.0."_s);
        QCOMPARE(offered.size(), 1);
    }

    void saysWhenYouHaveTheNewest()
    {
        Setup s;
        s.look({u"0.6.0"_s});
        QCOMPARE(s.updates.status(), u"You have the newest version."_s);
        QVERIFY(!s.updates.isPending());
    }

    void saysWhyItCouldNotLook()
    {
        Setup s;
        s.network.answers = {FakeAnswer{.status = 403}};
        s.updates.checkNow();
        QVERIFY(QTest::qWaitFor([&] { return !s.updates.isChecking(); }, 5000));
        QVERIFY(!s.updates.status().isEmpty());
        QVERIFY(!s.updates.status().startsWith(u"Version"_s));
        QVERIFY(!s.updates.isPending());
    }

    void betasAreYourChoice()
    {
        Setup s;
        QVERIFY(!s.updates.includesBetas());
        s.look({u"0.7.0-beta.1"_s});
        QVERIFY(!s.updates.isPending());
        s.updates.setIncludeBetas(true);
        QVERIFY(s.settings.settings().updates.includeBetas);
        QVERIFY(s.updates.isPending());
        QCOMPARE(s.updates.newVersion(), u"0.7.0-beta.1"_s);
    }

    void checkingOnLaunchIsSaved()
    {
        Setup s;
        QVERIFY(s.updates.checksOnLaunch());
        s.updates.setCheckOnLaunch(false);
        QVERIFY(!s.settings.settings().updates.checkOnLaunch);
        QVERIFY(!s.updates.checksOnLaunch());
    }

    void laterLeavesItForTheNextStart()
    {
        Setup s;
        s.look({u"0.7.0"_s});
        s.updates.later();
        QVERIFY(!s.updates.isPending());
        QVERIFY(s.settings.settings().updates.skipped.isEmpty());
        // asking again offers it again
        QSignalSpy offered(&s.updates, &Updates::offered);
        s.look({u"0.7.0"_s});
        QVERIFY(s.updates.isPending());
        QCOMPARE(offered.size(), 1);
    }

    void aNewerOneAfterLaterIsOffered()
    {
        Setup s;
        s.look({u"0.7.0"_s});
        s.updates.later();
        s.look({u"0.7.0"_s, u"0.8.0"_s});
        QVERIFY(s.updates.isPending());
        QCOMPARE(s.updates.newVersion(), u"0.8.0"_s);
    }

    void skippingIsKeptAndOnlyForThatVersion()
    {
        Setup s;
        s.look({u"0.7.0"_s});
        s.updates.skip();
        QCOMPARE(s.settings.settings().updates.skipped, u"0.7.0"_s);
        QVERIFY(!s.updates.isPending());
        // and a newer one still comes
        s.network.answers = {releases({u"0.7.0"_s, u"0.8.0"_s})};
        s.checker.check();
        QVERIFY(QTest::qWaitFor([&] { return !s.updates.isChecking(); }, 5000));
        QCOMPARE(s.updates.newVersion(), u"0.8.0"_s);
        QVERIFY(s.updates.isPending());
    }

    void checkNowOffersASkippedVersionAgain()
    {
        Setup s;
        s.look({u"0.7.0"_s});
        s.updates.skip();
        QVERIFY(!s.updates.isPending());
        s.look({u"0.7.0"_s});
        QVERIFY(s.settings.settings().updates.skipped.isEmpty());
        QVERIFY(s.updates.isPending());
    }

    void updateNowOpensTheReleasePage()
    {
        Setup s;
        s.updates.updateNow();
        QVERIFY(s.opener.opened.isEmpty());
        s.look({u"0.7.0"_s});
        s.updates.updateNow();
        QCOMPARE(s.opener.opened, QList<QUrl>{QUrl(releaseBase + u"/tag/v0.7.0"_s)});
        QVERIFY(!s.updates.isPending());
    }

    void nothingToDoWithoutARelease()
    {
        Setup s;
        s.updates.later();
        s.updates.skip();
        QVERIFY(s.settings.settings().updates.skipped.isEmpty());
        QVERIFY(s.opener.opened.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestUpdatesModel)
#include "tst_updatesmodel.moc"
