// SPDX-License-Identifier: MIT
#include "fakenetwork.h"
#include "services/updatedownloader.h"
#include "updatetexts.h"

#include <QCryptographicHash>
#include <QFile>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

Q_DECLARE_METATYPE(FakeAnswer)

namespace {

const QByteArray package = "pretend this is a package";

Release release(const QString &version = u"0.8.0"_s)
{
    const QList<Release> list = readReleases(fakeReleases({fakeRelease(version)}));
    return list.first();
}

// What SHA256SUMS says about a file.
FakeAnswer sums(const QString &name, const QByteArray &content)
{
    const QByteArray sum = QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex();
    return {.body = sum + "  " + name.toUtf8() + "\n"};
}

struct Setup
{
    QTemporaryDir dir;
    FakeNetwork network;
    UpdateDownloader downloader{&network, dir.filePath(u"updates"_s)};
    QSignalSpy done{&downloader, &UpdateDownloader::done};
    QSignalSpy failed{&downloader, &UpdateDownloader::failed};

    QString name(InstallKind kind = InstallKind::Debian) const { return assetFor(release(), kind)->name; }

    bool settles()
    {
        return QTest::qWaitFor([&] { return !downloader.isRunning(); }, 5000);
    }
};

} // namespace

class TestUpdateDownloader : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLoggingCategory::setFilterRules(u"pathofupgrading.updates.warning=false"_s); }

    void downloadsAndChecksTheFile()
    {
        Setup s;
        s.network.answers = {sums(s.name(), package), {.body = package}};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.downloader.isRunning());
        QVERIFY(s.settles());
        QCOMPARE(s.failed.size(), 0);
        QCOMPARE(s.done.size(), 1);
        const QString file = s.done.at(0).at(0).toString();
        QCOMPARE(file, s.dir.filePath(u"updates/0.8.0/"_s + s.name()));
        QFile saved(file);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), package);

        // the checksums first, then the file, as this project's release
        QCOMPARE(s.network.sent.size(), 2);
        QCOMPARE(s.network.sent.at(0).request.url().fileName(), u"SHA256SUMS"_s);
        QCOMPARE(s.network.sent.at(1).request.url().fileName(), s.name());
        QVERIFY(s.network.sent.at(1)
                    .request.header(QNetworkRequest::UserAgentHeader)
                    .toByteArray()
                    .startsWith("PathOfUpgrading/"));
    }

    void theFolderIsYoursAlone()
    {
#ifdef Q_OS_WIN
        QSKIP("Windows has no group and other permissions to take away");
#endif
        Setup s;
        s.network.answers = {sums(s.name(), package), {.body = package}};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.settles());
        const QFileDevice::Permissions folder = QFile::permissions(s.dir.filePath(u"updates/0.8.0"_s));
        QVERIFY(!(folder & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup |
                            QFileDevice::WriteOther)));
    }

    void aFileThatDoesNotMatchIsThrownAway()
    {
        Setup s;
        s.network.answers = {sums(s.name(), package), {.body = "something else"}};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.settles());
        QCOMPARE(s.done.size(), 0);
        QCOMPARE(s.failed.size(), 1);
        QCOMPARE(s.failed.at(0).at(0).toString(),
                 u"The download didn't match its checksum, so it was thrown away."_s);
        QVERIFY(!QFile::exists(s.dir.filePath(u"updates/0.8.0/"_s + s.name())));
    }

    void aFileTheChecksumsDoNotListFails()
    {
        Setup s;
        s.network.answers = {sums(u"another-file.deb"_s, package), {.body = package}};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.settles());
        QCOMPARE(s.done.size(), 0);
        QCOMPARE(s.failed.size(), 1);
        // and the file was never asked for
        QCOMPARE(s.network.sent.size(), 1);
    }

    void aFileBiggerThanTheReleaseSaysFails()
    {
        Setup s;
        const QByteArray big(2 * 1024 * 1024, 'x');
        s.network.answers = {sums(s.name(), big), {.body = big}};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.settles());
        QCOMPARE(s.done.size(), 0);
        QCOMPARE(s.failed.size(), 1);
        QVERIFY(!QFile::exists(s.dir.filePath(u"updates/0.8.0/"_s + s.name())));
    }

    void failuresAreSaidInWords_data()
    {
        QTest::addColumn<FakeAnswer>("answer");
        QTest::addColumn<QString>("why");
        QTest::newRow("not found") << FakeAnswer{.status = 404}
                                   << u"GitHub didn't send the checksums (HTTP 404)."_s;
        QTest::newRow("unreachable") << FakeAnswer{.error = QNetworkReply::HostNotFoundError}
                                     << u"Couldn't reach GitHub: Host github.com not found"_s;
    }

    void failuresAreSaidInWords()
    {
        QFETCH(FakeAnswer, answer);
        QFETCH(QString, why);
        Setup s;
        s.network.answers = {answer};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.settles());
        QCOMPARE(s.failed.size(), 1);
        QCOMPARE(s.failed.at(0).at(0).toString(), why);
    }

    void theFileFailingIsSaidToo()
    {
        Setup s;
        s.network.answers = {sums(s.name(), package), FakeAnswer{.status = 500}};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.settles());
        QCOMPARE(s.failed.at(0).at(0).toString(), u"GitHub didn't send the file (HTTP 500)."_s);
        QVERIFY(!QFile::exists(s.dir.filePath(u"updates/0.8.0/"_s + s.name())));
    }

    void noFileForHowItWasInstalled()
    {
        Setup s;
        s.downloader.start(release(), InstallKind::Source);
        QCOMPARE(s.failed.size(), 1);
        QVERIFY(!s.downloader.isRunning());
        QVERIFY(s.network.sent.isEmpty());
    }

    void noChecksumsNoDownload()
    {
        Setup s;
        QJsonObject json = fakeRelease(u"0.8.0"_s);
        QJsonArray assets;
        for (const QJsonValue &asset : json[u"assets"_s].toArray()) {
            if (asset.toObject()[u"name"_s].toString() != u"SHA256SUMS"_s)
                assets.append(asset);
        }
        json[u"assets"_s] = assets;
        s.downloader.start(readReleases(fakeReleases({json})).first(), InstallKind::Debian);
        QCOMPARE(s.failed.size(), 1);
        QVERIFY(s.network.sent.isEmpty());
    }

    void cancelLeavesNothing()
    {
        Setup s;
        s.network.answers = {sums(s.name(), package), {.body = package}};
        s.downloader.start(release(), InstallKind::Debian);
        s.downloader.cancel();
        QVERIFY(!s.downloader.isRunning());
        QVERIFY(!QFile::exists(s.dir.filePath(u"updates"_s)));
        QCOMPARE(s.done.size(), 0);
        QCOMPARE(s.failed.size(), 0);
        // and it can go again
        s.network.answers = {sums(s.name(), package), {.body = package}};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.settles());
        QCOMPARE(s.done.size(), 1);
    }

    void aNewDownloadEmptiesTheFolder()
    {
        Setup s;
        QDir().mkpath(s.dir.filePath(u"updates/0.7.0"_s));
        QFile old(s.dir.filePath(u"updates/0.7.0/old.deb"_s));
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.close();
        s.network.answers = {sums(s.name(), package), {.body = package}};
        s.downloader.start(release(), InstallKind::Debian);
        QVERIFY(s.settles());
        QVERIFY(!QFile::exists(s.dir.filePath(u"updates/0.7.0/old.deb"_s)));
    }

    void noFolderNoDownload()
    {
        FakeNetwork network;
        UpdateDownloader downloader(&network, QString());
        QSignalSpy failed(&downloader, &UpdateDownloader::failed);
        downloader.start(release(), InstallKind::Debian);
        QCOMPARE(failed.size(), 1);
        QVERIFY(network.sent.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestUpdateDownloader)
#include "tst_updatedownloader.moc"
