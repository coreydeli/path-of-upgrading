// SPDX-License-Identifier: MIT
#include "core/updates.h"
#include "updatetexts.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

Version v(const QString &text)
{
    return parseVersion(text).value_or(Version{});
}

} // namespace

class TestUpdates : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void versionsAreReadAsTagsAreMade()
    {
        QCOMPARE(v(u"v0.7.0"_s).numbers, (std::array<int, 3>{0, 7, 0}));
        QCOMPARE(v(u"0.7.0"_s).beta, -1);
        QCOMPARE(v(u"v0.7.0-beta.4"_s).beta, 4);
        QCOMPARE(v(u"v0.7.0-beta.4"_s).text(), u"0.7.0-beta.4"_s);
        QCOMPARE(v(u"10.20.30"_s).text(), u"10.20.30"_s);
        for (const QString &bad :
             {u""_s, u"v1"_s, u"v1.2"_s, u"v1.2.3.4"_s, u"v1.2.3-rc.1"_s, u"v1.2.3-beta"_s,
              u"v1.2.3-beta.x"_s, u"latest"_s, u"v1.2.3 "_s, u"v-1.2.3"_s, u"v99999999999.0.0"_s})
            QVERIFY2(!parseVersion(bad), qPrintable(bad));
    }

    void betasComeBeforeTheirRelease()
    {
        QVERIFY(v(u"0.7.0-beta.4"_s) < v(u"0.7.0"_s));
        QVERIFY(v(u"0.7.0-beta.9"_s) < v(u"0.7.0-beta.10"_s));
        QVERIFY(v(u"0.7.0"_s) < v(u"0.7.1-beta.1"_s));
        QVERIFY(v(u"0.6.0"_s) < v(u"0.7.0-beta.1"_s));
        QVERIFY(v(u"0.9.0"_s) < v(u"0.10.0"_s));
        QVERIFY(v(u"1.0.0-beta.2"_s) > v(u"0.99.99"_s));
        QCOMPARE(v(u"v0.7.0"_s), v(u"0.7.0"_s));
    }

    void theBuildKnowsItsVersion()
    {
        QCOMPARE(buildVersion().text(), QString::fromLatin1(POU_EXPECT_VERSION));
    }

    void readsTheReleases()
    {
        const QList<Release> releases =
            readReleases(fakeReleases({fakeRelease(u"0.7.0-beta.4"_s, true), fakeRelease(u"0.6.0"_s)}));
        QCOMPARE(releases.size(), 2);
        const Release &beta = releases.at(0);
        QCOMPARE(beta.tag, u"v0.7.0-beta.4"_s);
        QCOMPARE(beta.version, v(u"0.7.0-beta.4"_s));
        QVERIFY(beta.isBeta());
        QCOMPARE(beta.notes, u"What is new in 0.7.0-beta.4."_s);
        QCOMPARE(beta.page, QUrl(releaseBase + u"/tag/v0.7.0-beta.4"_s));
        QCOMPARE(beta.assets.size(), 8);
        QCOMPARE(beta.assets.at(0).size, 1234);
        QVERIFY(!releases.at(1).isBeta());
    }

    void leavesOutWhatIsntARelease()
    {
        QJsonObject odd = fakeRelease(u"0.8.0"_s);
        odd[u"tag_name"_s] = u"nightly"_s;
        const QList<Release> releases =
            readReleases(fakeReleases({fakeRelease(u"0.9.0"_s, false, true), odd, fakeRelease(u"0.7.0"_s)}));
        QCOMPARE(releases.size(), 1);
        QCOMPARE(releases.at(0).tag, u"v0.7.0"_s);

        QVERIFY(readReleases("").isEmpty());
        QVERIFY(readReleases("{}").isEmpty());
        QVERIFY(readReleases("[1, \"two\", null]").isEmpty());
    }

    void takesOnlyThisProjectsReleasesOverHttps_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("ok");
        QTest::newRow("a file") << releaseBase + u"/download/v1.0.0/a.AppImage"_s << true;
        QTest::newRow("the page") << releaseBase + u"/tag/v1.0.0"_s << true;
        QTest::newRow("plain http")
            << u"http://github.com/coreydeli/path-of-upgrading/releases/download/v1/a"_s << false;
        QTest::newRow("another host")
            << u"https://example.com/coreydeli/path-of-upgrading/releases/a"_s << false;
        QTest::newRow("a look alike host")
            << u"https://github.com.example.com/coreydeli/path-of-upgrading/releases/a"_s << false;
        QTest::newRow("a user at another host")
            << u"https://github.com@example.com/coreydeli/path-of-upgrading/releases/a"_s << false;
        QTest::newRow("another repository")
            << u"https://github.com/someone/else/releases/download/v1/a"_s << false;
        QTest::newRow("outside releases")
            << u"https://github.com/coreydeli/path-of-upgrading/raw/main/a"_s << false;
        QTest::newRow("another port")
            << u"https://github.com:8443/coreydeli/path-of-upgrading/releases/a"_s << false;
        QTest::newRow("nothing") << u""_s << false;
    }

    void takesOnlyThisProjectsReleasesOverHttps()
    {
        QFETCH(QString, url);
        QFETCH(bool, ok);
        QCOMPARE(isReleaseUrl(QUrl(url)), ok);
    }

    void dropsFilesFromElsewhere()
    {
        QJsonObject release = fakeRelease(u"0.7.0"_s);
        QJsonArray assets = release[u"assets"_s].toArray();
        QJsonObject far = fakeAsset(u"v0.7.0"_s, u"evil.exe"_s);
        far[u"browser_download_url"_s] = u"https://example.com/evil.exe"_s;
        QJsonObject sneaky = fakeAsset(u"v0.7.0"_s, u"../../evil.exe"_s);
        assets.append(far);
        assets.append(sneaky);
        release[u"assets"_s] = assets;
        const QList<Release> releases = readReleases(fakeReleases({release}));
        QCOMPARE(releases.at(0).assets.size(), 8);
        for (const ReleaseAsset &asset : releases.at(0).assets)
            QVERIFY(!asset.name.contains(u"evil"_s));

        QJsonObject elsewhere = fakeRelease(u"0.7.0"_s);
        elsewhere[u"html_url"_s] = u"https://example.com/"_s;
        QVERIFY(readReleases(fakeReleases({elsewhere})).at(0).page.isEmpty());
    }

    void picksTheNewestAllowed()
    {
        const QList<Release> releases = readReleases(
            fakeReleases({fakeRelease(u"0.7.0-beta.4"_s, true), fakeRelease(u"0.7.0-beta.10"_s, true),
                          fakeRelease(u"0.6.0"_s), fakeRelease(u"0.7.0-beta.9"_s, true)}));
        // betas in whatever order
        QCOMPARE(newerRelease(releases, v(u"0.6.0"_s), true)->tag, u"v0.7.0-beta.10"_s);
        // and without them, only a release counts
        QVERIFY(!newerRelease(releases, v(u"0.6.0"_s), false));
        QVERIFY(!newerRelease(releases, v(u"0.7.0-beta.10"_s), true));
        QCOMPARE(newerRelease(releases, v(u"0.7.0-beta.9"_s), true)->tag, u"v0.7.0-beta.10"_s);
    }

    void aBetaNeverGoesBack()
    {
        // a beta of 0.7.0 with betas off waits for the release
        QList<Release> releases =
            readReleases(fakeReleases({fakeRelease(u"0.7.0-beta.5"_s, true), fakeRelease(u"0.6.0"_s)}));
        QVERIFY(!newerRelease(releases, v(u"0.7.0-beta.4"_s), false));
        releases = readReleases(fakeReleases({fakeRelease(u"0.7.0"_s), fakeRelease(u"0.6.0"_s)}));
        QCOMPARE(newerRelease(releases, v(u"0.7.0-beta.4"_s), false)->tag, u"v0.7.0"_s);
        // and the release is no update to itself
        QVERIFY(!newerRelease(releases, v(u"0.7.0"_s), true));
    }

    void aPrereleaseFlagCountsAsABeta()
    {
        const QList<Release> releases = readReleases(fakeReleases({fakeRelease(u"0.8.0"_s, true)}));
        QVERIFY(!newerRelease(releases, v(u"0.7.0"_s), false));
        QVERIFY(newerRelease(releases, v(u"0.7.0"_s), true));
    }

    void findsTheFileForHowItWasInstalled_data()
    {
        QTest::addColumn<InstallKind>("kind");
        QTest::addColumn<QString>("name");
        QTest::newRow("appimage") << InstallKind::AppImage
                                  << u"PathOfUpgrading-0.7.0-beta.4-x86_64.AppImage"_s;
        QTest::newRow("windows") << InstallKind::Windows << u"path-of-upgrading-0.7.0-beta.4-setup.exe"_s;
        QTest::newRow("arch") << InstallKind::Arch << u"path-of-upgrading-0.7.0beta4-1-x86_64.pkg.tar.zst"_s;
        QTest::newRow("debian") << InstallKind::Debian << u"path-of-upgrading_0.7.0.beta.4-1_amd64.deb"_s;
        QTest::newRow("fedora") << InstallKind::Fedora
                                << u"path-of-upgrading-0.7.0.beta.4-1.fc45.x86_64.rpm"_s;
    }

    void findsTheFileForHowItWasInstalled()
    {
        QFETCH(InstallKind, kind);
        QFETCH(QString, name);
        const Release release = readReleases(fakeReleases({fakeRelease(u"0.7.0-beta.4"_s, true)})).at(0);
        const std::optional<ReleaseAsset> asset = assetFor(release, kind);
        QVERIFY(asset);
        QCOMPARE(asset->name, name);
        QVERIFY(isReleaseUrl(asset->url));
    }

    void builtFromSourceHasNoFile()
    {
        const Release release = readReleases(fakeReleases({fakeRelease(u"0.7.0"_s)})).at(0);
        QVERIFY(!assetFor(release, InstallKind::Source));
        // nor does a release without the file
        Release bare = release;
        bare.assets.clear();
        QVERIFY(!assetFor(bare, InstallKind::Arch));
        QVERIFY(!checksumsOf(bare));
        QCOMPARE(checksumsOf(release)->name, u"SHA256SUMS"_s);
    }

    void installKindsHaveIds()
    {
        for (InstallKind kind : {InstallKind::Source, InstallKind::Arch, InstallKind::Debian,
                                 InstallKind::Fedora, InstallKind::AppImage, InstallKind::Windows})
            QCOMPARE(installKindFromId(installKindId(kind)), kind);
        QCOMPARE(installKindFromId(u"gentoo"_s), InstallKind::Source);
        QCOMPARE(installKindFromId(u""_s), InstallKind::Source);
    }

    void readsTheChecksums()
    {
        const QString a(64, u'a');
        const QString b = QString(63, u'B') + u'1';
        const QByteArray text = (a + u"  one.AppImage\n"_s + b + u" *two file.exe\r\n"_s +
                                 u"\nnot a line\n"_s + QString(63, u'c') + u"  short.txt\n"_s)
                                    .toUtf8();
        const QHash<QString, QString> sums = parseChecksums(text);
        QCOMPARE(sums.size(), 2);
        QCOMPARE(sums.value(u"one.AppImage"_s), a);
        // in lower case
        QCOMPARE(sums.value(u"two file.exe"_s), b.toLower());
        QVERIFY(parseChecksums("").isEmpty());
    }

    void notesAreReadableText()
    {
        const QString markdown =
            u"## What's Changed\n* **Fixed** the `popup` at [the mouse](https://example.org/x)\n"
            u"* <b>Added</b> a thing\n\n\n\n1. First\r\n"_s;
        QCOMPARE(plainNotes(markdown),
                 u"What's Changed\nFixed the popup at the mouse\nAdded a thing\n\nFirst"_s);
        QVERIFY(plainNotes(QString()).isEmpty());
    }

    void longNotesAreCutAtAWord()
    {
        const QString words = u"word "_s.repeated(100);
        const QString cut = plainNotes(words, 50);
        QVERIFY(cut.startsWith(u"word word"_s));
        QVERIFY(cut.endsWith(u"\nThe rest is on the release page."_s));
        QVERIFY(cut.size() < 100);
        QCOMPARE(plainNotes(u"short"_s, 50), u"short"_s);
    }
};

QTEST_GUILESS_MAIN(TestUpdates)
#include "tst_updates.moc"
