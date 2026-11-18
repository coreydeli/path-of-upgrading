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
    void removesFilesAndFoldersWithWhatIsInThem()
    {
        QTemporaryDir dir;
        const QString settings = dir.filePath(u"made-uprc"_s);
        const QString data = dir.filePath(u"data"_s);
        QVERIFY(write(settings));
        QVERIFY(write(data + u"/characters/one.json"_s));
        QVERIFY(write(data + u"/history.json"_s));
        QVERIFY(write(dir.filePath(u"beside/builds/mine.xml"_s)));
        QVERIFY(write(dir.filePath(u"other-rc"_s)));

        const QStringList problems =
            removeOwnPaths({{.what = u"settings"_s, .path = settings}, {.what = u"data"_s, .path = data}});

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
        QVERIFY(removeOwnPaths({{.what = u"data"_s, .path = dir.filePath(u"never-made"_s)}}).isEmpty());
    }

    void oneThatCantGoDoesntStopTheRest()
    {
        QTemporaryDir dir;
        // a folder with a file can't have the file removed when it can't be written to
        const QString locked = dir.filePath(u"locked"_s);
        QVERIFY(write(locked + u"/kept.json"_s));
        const QString cache = dir.filePath(u"cache"_s);
        QVERIFY(write(cache + u"/rates.json"_s));
        QVERIFY(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        if (QFile::remove(locked + u"/probe"_s) || QFileInfo(locked).isWritable()) {
            QFile::setPermissions(locked,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            QSKIP("This account can write to a folder that's read only.");
        }

        const QStringList problems = removeOwnPaths(
            {{.what = u"your data"_s, .path = locked}, {.what = u"your cache"_s, .path = cache}});

        QCOMPARE(problems.size(), 1);
        QVERIFY(problems.first().contains(u"your data"_s));
        QVERIFY(problems.first().contains(locked));
        QVERIFY(QFileInfo::exists(locked + u"/kept.json"_s));
        QVERIFY(!QFileInfo::exists(cache));
        QVERIFY(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                  QFileDevice::ExeOwner));
    }

    void neverTouchesTheRootTheHomeFolderOrNothing()
    {
        const QStringList problems = removeOwnPaths({{.what = u"a"_s, .path = QString()},
                                                     {.what = u"b"_s, .path = u"/"_s},
                                                     {.what = u"c"_s, .path = QDir::homePath()}});
        QCOMPARE(problems.size(), 3);
        QVERIFY(QFileInfo::exists(QDir::homePath()));
    }

    void aLinkIsRemovedAndWhatItPointsAtIsNot()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(u"elsewhere"_s);
        QVERIFY(write(target + u"/keep.txt"_s));
        const QString link = dir.filePath(u"link"_s);
        QVERIFY(QFile::link(target, link));
        QVERIFY(removeOwnPaths({{.what = u"data"_s, .path = link}}).isEmpty());
        QVERIFY(!QFileInfo(link).isSymLink());
        QVERIFY(QFileInfo::exists(target + u"/keep.txt"_s));
    }
};

QTEST_GUILESS_MAIN(TestOwnPaths)
#include "tst_ownpaths.moc"
