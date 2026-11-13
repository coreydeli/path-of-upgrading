// SPDX-License-Identifier: MIT
#include "app/about.h"
#include "app/poboptions.h"
#include "app/settingsstore.h"
#include "core/settings.h"
#include "fakeopener.h"
#include "services/pobengine.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

struct Pieces
{
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    PobEngine pob;
    PobPlaces places{.home = dir.path(), .dataHome = dir.filePath(u"data"_s)};
    FakeOpener opener;
    PobOptions options{&settings, &pob, places, &opener};
    About about{&options, &opener};
};

} // namespace

class TestAbout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void versionIsTheApplications()
    {
        Pieces p;
        const QString was = QCoreApplication::applicationVersion();
        QCoreApplication::setApplicationVersion(u"9.8.7"_s);
        QCOMPARE(p.about.version(), u"9.8.7"_s);
        QCoreApplication::setApplicationVersion(QString());
        QCOMPARE(p.about.version(), u"unknown"_s);
        QCoreApplication::setApplicationVersion(was);
    }

    void saysWhichKind()
    {
        Pieces p;
        QCOMPARE(p.about.edition(), isBetaBuild() ? u"Beta"_s : u"Release"_s);
        QCOMPARE(p.about.license(), u"MIT"_s);
        QCOMPARE(p.about.qtVersion(), QString::fromLatin1(qVersion()));
        QVERIFY(!p.about.frameworksVersion().isEmpty());
    }

    void pathOfBuilding()
    {
        Pieces p;
        // nowhere to look
        QCOMPARE(p.about.pobStatus(), u"Not found"_s);
        QSignalSpy changed(&p.about, &About::pobStatusChanged);
        p.pob.setUnavailable(u"Nothing."_s);
        QVERIFY(changed.size() > 0);
    }

    void creditsHaveLinks()
    {
        Pieces p;
        const QVariantList credits = p.about.credits();
        QVERIFY(credits.size() >= 4);
        QStringList names;
        for (const QVariant &credit : credits) {
            const QVariantMap map = credit.toMap();
            names << map.value(u"name"_s).toString();
            QVERIFY(map.value(u"url"_s).toString().startsWith(u"https://"_s));
        }
        QVERIFY(names.contains(u"Big Shoulders"_s));
        QVERIFY(names.contains(u"Spectral"_s));
        QVERIFY(!names.contains(u"Cinzel"_s));
        QVERIFY(names.contains(u"Path of Building Community"_s));
    }

    void linksOpenOnlyWhenAsked()
    {
        Pieces p;
        QVERIFY(p.opener.opened.isEmpty());
        p.about.openRepository();
        p.about.openReleases();
        p.about.openIssues();
        QCOMPARE(p.opener.opened,
                 (QList<QUrl>{QUrl(u"https://github.com/coreydeli/path-of-upgrading"_s),
                              QUrl(u"https://github.com/coreydeli/path-of-upgrading/releases"_s),
                              QUrl(u"https://github.com/coreydeli/path-of-upgrading/issues"_s)}));

        p.opener.opened.clear();
        p.about.openCredit(0);
        QCOMPARE(p.opener.opened.size(), 1);
        p.about.openCredit(-1);
        p.about.openCredit(100);
        QCOMPARE(p.opener.opened.size(), 1);
    }

    void withoutAnOpener()
    {
        Pieces p;
        About about(&p.options, nullptr);
        about.openRepository();
        about.openCredit(0);
    }
};

QTEST_MAIN(TestAbout)
#include "tst_about.moc"
