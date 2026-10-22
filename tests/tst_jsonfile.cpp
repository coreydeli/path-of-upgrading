// SPDX-License-Identifier: MIT
#include "core/jsonfile.h"

#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestJsonFile : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(u"character.json"_s);
        const QJsonObject object = {{u"name"_s, u"Ember"_s}, {u"level"_s, 92}};
        QVERIFY(writeJsonFile(path, object));
        QCOMPARE(readJsonFile(path).value(), object);

        // replaced, with nothing left over from the old one or the writing
        QVERIFY(writeJsonFile(path, {{u"name"_s, u"Frost"_s}}));
        QCOMPARE(readJsonFile(path).value(), (QJsonObject{{u"name"_s, u"Frost"_s}}));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{u"character.json"_s});
    }

    void makesTheFolder()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"characters/one.json"_s);
        QVERIFY(writeJsonFile(path, {}));
        QVERIFY(QFile::exists(path));
    }

    void unreadable()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!readJsonFile(dir.filePath(u"missing.json"_s), &error));
        QVERIFY(!error.isEmpty());

        const auto writeRaw = [&](const QString &name, const QByteArray &data) {
            QFile file(dir.filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(writeRaw(u"cut.json"_s, "{\"name\": \"Em"));
        error.clear();
        QVERIFY(!readJsonFile(dir.filePath(u"cut.json"_s), &error));
        QVERIFY(!error.isEmpty());

        QVERIFY(writeRaw(u"list.json"_s, "[1, 2]"));
        QVERIFY(!readJsonFile(dir.filePath(u"list.json"_s), &error));
        QCOMPARE(error, u"not a JSON object"_s);
    }

    void unwritable()
    {
        QTemporaryDir dir;
        const QString locked = dir.filePath(u"locked"_s);
        QVERIFY(QDir().mkdir(locked));
        QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
        // so the temporary folder can go again
        const auto unlock = qScopeGuard(
            [&] { QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
        const QString path = locked + u"/character.json"_s;
        QFile probe(path);
        if (probe.open(QIODevice::WriteOnly))
            QSKIP("the folder can be written to anyway, as root");

        QString error;
        QVERIFY(!writeJsonFile(path, {{u"name"_s, u"Ember"_s}}, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
    }
};

QTEST_GUILESS_MAIN(TestJsonFile)
#include "tst_jsonfile.moc"
