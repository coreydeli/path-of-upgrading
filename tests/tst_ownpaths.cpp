// SPDX-License-Identifier: MIT
#include "services/ownpaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// What the removal asks for in the name of every path these tests make, so
// only what they make in their own temporary folder is ever an own path.
const QString marker = u"made-up-app"_s;

bool write(const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("made up") > 0;
}

} // namespace

class TestOwnPaths : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // These only read text, and never go near the file system.
    void ownPathsAreAcceptedByTheirName()
    {
        const QString m = u"path-of-upgrading"_s;
        QVERIFY(isOwnPath(u"/home/someone/.local/share/path-of-upgrading"_s, m));
        QVERIFY(isOwnPath(u"/home/someone/.cache/path-of-upgrading"_s, m));
        QVERIFY(isOwnPath(u"/home/someone/.config/path-of-upgradingrc"_s, m));
        QVERIFY(isOwnPath(u"C:/Users/someone/AppData/Roaming/path-of-upgrading"_s, m));
        QVERIFY(isOwnPath(u"C:\\Users\\someone\\AppData\\Roaming\\path-of-upgrading"_s, m));
        QVERIFY(isOwnPath(u"/home/someone/.local/share/path-of-upgrading/"_s, m));
    }

    void rootsAndSharesAreRefused()
    {
        const QString m = u"path-of-upgrading"_s;
        for (const QString &path :
             {u"/"_s, u"C:/"_s, u"D:/"_s, u"D:"_s, u"C:\\"_s, u"//"_s, u"\\\\server\\share"_s,
              u"//server/share"_s, u"//server/share/path-of-upgrading"_s})
            QVERIFY2(!isOwnPath(path, m), qPrintable(path));
    }

    void whatIsntAbsoluteOrIsEmptyIsRefused()
    {
        const QString m = u"path-of-upgrading"_s;
        for (const QString &path :
             {QString(), u" "_s, u"."_s, u".."_s, u"path-of-upgrading"_s, u"data/path-of-upgrading"_s,
              u"../../path-of-upgrading"_s, u"~/path-of-upgrading"_s})
            QVERIFY2(!isOwnPath(path, m), qPrintable(path));
        // an empty marker would match anything
        QVERIFY(!isOwnPath(u"/home/someone/.local/share/path-of-upgrading"_s, QString()));
    }

    void foldersAboveOrBesideTheAppsAreRefused()
    {
        const QString m = u"path-of-upgrading"_s;
        QVERIFY(!isOwnPath(u"/home/someone/.local/share"_s, m));
        QVERIFY(!isOwnPath(u"/home/someone"_s, m));
        QVERIFY(!isOwnPath(u"/home/someone/.local/share/other-app"_s, m));
        QVERIFY(!isOwnPath(u"/home/someone/.local/share/path-of-upgrading/data"_s, m));
        QVERIFY(!isOwnPath(u"C:/Users/someone/AppData/Roaming"_s, m));
        QVERIFY(!isOwnPath(u"/path-of-upgrading"_s, m));
        QVERIFY(!isOwnPath(u"C:/path-of-upgrading"_s, m));
        // the .. is taken out first, which leaves a top folder
        QVERIFY(!isOwnPath(u"/home/someone/../../path-of-upgrading"_s, m));
        QVERIFY(!isOwnPath(u"/home/someone/.local/share/path-of-upgrading/.."_s, m));
    }

    void yourHomeFolderAndTheOnesAboveItAreRefused()
    {
        const QString home = QDir::homePath();
        // with the marker being its own name, so only the home rule can refuse it
        QVERIFY(!isOwnPath(home, QFileInfo(home).fileName()));
        QVERIFY(!isOwnPath(home + u'/', QFileInfo(home).fileName()));
        QVERIFY(
            !isOwnPath(QFileInfo(home).absolutePath(), QFileInfo(QFileInfo(home).absolutePath()).fileName()));
    }

    void theCacheOnWindowsIsTheAppsFolderItIsIn()
    {
        const QString m = u"path-of-upgrading"_s;
        QCOMPARE(ownFolderOf(u"C:/Users/someone/AppData/Local/path-of-upgrading/cache"_s, m),
                 u"C:/Users/someone/AppData/Local/path-of-upgrading"_s);
        QCOMPARE(ownFolderOf(u"C:\\Users\\someone\\AppData\\Local\\path-of-upgrading\\cache"_s, m),
                 u"C:/Users/someone/AppData/Local/path-of-upgrading"_s);
        // one that is its own is kept as it is
        QCOMPARE(ownFolderOf(u"/home/someone/.cache/path-of-upgrading"_s, m),
                 u"/home/someone/.cache/path-of-upgrading"_s);
    }

    void aFolderTwoDownOrInAnotherAppsIsntTakenUp()
    {
        const QString m = u"path-of-upgrading"_s;
        for (const QString &path :
             {u"C:/Users/someone/AppData/Local/path-of-upgrading/cache/trade"_s,
              u"C:/Users/someone/AppData/Local/other-app/cache"_s, u"C:/Users/someone/AppData/Local"_s,
              u"/path-of-upgrading/cache"_s, u"C:/path-of-upgrading/cache"_s,
              u"data/path-of-upgrading/cache"_s, u"//server/path-of-upgrading/cache"_s, QString()}) {
            QCOMPARE(ownFolderOf(path, m), path);
            QVERIFY2(!isOwnPath(ownFolderOf(path, m), m), qPrintable(path));
        }
    }

    // From here on, only what these make in a temporary folder is touched.
    void removesFilesAndFoldersWithWhatIsInThem()
    {
        QTemporaryDir dir;
        const QString settings = dir.filePath(u"made-up-apprc"_s);
        const QString data = dir.filePath(u"made-up-app"_s);
        QVERIFY(write(settings));
        QVERIFY(write(data + u"/characters/one.json"_s));
        QVERIFY(write(data + u"/history.json"_s));
        QVERIFY(write(dir.filePath(u"beside/builds/mine.xml"_s)));
        QVERIFY(write(dir.filePath(u"other-rc"_s)));

        const QStringList problems = removeOwnPaths(
            {{.what = u"settings"_s, .path = settings}, {.what = u"data"_s, .path = data}}, marker);

        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(u'\n')));
        QVERIFY(!QFileInfo::exists(settings));
        QVERIFY(!QFileInfo::exists(data));
        // what's next to them is left as it was
        QVERIFY(QFileInfo::exists(dir.filePath(u"beside/builds/mine.xml"_s)));
        QVERIFY(QFileInfo::exists(dir.filePath(u"other-rc"_s)));
    }

    void whatIsAlreadyGoneIsFine()
    {
        QTemporaryDir dir;
        QVERIFY(removeOwnPaths({{.what = u"data"_s, .path = dir.filePath(u"made-up-app-never"_s)}}, marker)
                    .isEmpty());
    }

    void aPathThatIsntTheAppsIsLeftAlone()
    {
        QTemporaryDir dir;
        const QString other = dir.filePath(u"someone-elses/keep.txt"_s);
        QVERIFY(write(other));
        const QStringList problems =
            removeOwnPaths({{.what = u"data"_s, .path = QFileInfo(other).absolutePath()},
                            {.what = u"x"_s, .path = QString()}},
                           marker);
        QCOMPARE(problems.size(), 2);
        QVERIFY(problems.first().contains(u"Left data alone"_s));
        QVERIFY(QFileInfo::exists(other));
    }

    void oneThatCantGoDoesntStopTheRest()
    {
        QTemporaryDir dir;
        // a folder with a file can't have the file removed when it can't be written to
        const QString locked = dir.filePath(u"made-up-app-locked"_s);
        QVERIFY(write(locked + u"/kept.json"_s));
        const QString cache = dir.filePath(u"made-up-app-cache"_s);
        QVERIFY(write(cache + u"/rates.json"_s));
        const auto allowed = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
        QVERIFY(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        if (QFileInfo(locked).isWritable()) {
            QFile::setPermissions(locked, allowed);
            QSKIP("This account can write to a folder that's read only.");
        }

        const QStringList problems = removeOwnPaths(
            {{.what = u"your data"_s, .path = locked}, {.what = u"your cache"_s, .path = cache}}, marker);
        QVERIFY(QFile::setPermissions(locked, allowed));

        QCOMPARE(problems.size(), 1);
        QVERIFY(problems.first().contains(u"your data"_s));
        QVERIFY(problems.first().contains(locked));
        QVERIFY(QFileInfo::exists(locked + u"/kept.json"_s));
        QVERIFY(!QFileInfo::exists(cache));
    }

    void aLinkIsRemovedAndWhatItPointsAtIsNot()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"elsewhere"_s);
        QVERIFY(write(target + u"/keep.txt"_s));
        const QString link = dir.filePath(u"made-up-app-link"_s);
        if (!QFile::link(target, link))
            QSKIP("Links can't be made here.");
        QVERIFY(removeOwnPaths({{.what = u"data"_s, .path = link}}, marker).isEmpty());
        QVERIFY(!QFileInfo(link).isSymLink());
        QVERIFY(QFileInfo::exists(target + u"/keep.txt"_s));
    }
};

QTEST_GUILESS_MAIN(TestOwnPaths)
#include "tst_ownpaths.moc"
