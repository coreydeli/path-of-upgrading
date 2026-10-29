// SPDX-License-Identifier: MIT
#include "platform/keychain/keychainsecretstore.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

// Only what the keyring's errors come to, as the keyring itself is the
// system's and tests don't ask it anything.
class TestKeychainSecretStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void done()
    {
        const SecretStore::Result result = KeychainSecretStore::resultOf(QKeychain::NoError, {});
        QCOMPARE(result.status, SecretStore::Status::Done);
        QVERIFY(result.error.isEmpty());
    }

    void notFound()
    {
        // which isn't a failure
        const SecretStore::Result result = KeychainSecretStore::resultOf(QKeychain::EntryNotFound, {});
        QCOMPARE(result.status, SecretStore::Status::NotFound);
        QVERIFY(result.error.isEmpty());
    }

    void failed_data()
    {
        QTest::addColumn<int>("error");
        QTest::addColumn<QString>("why");
        QTest::addColumn<QString>("expected");
        const QString denied = u"The system keyring wouldn't let Path of Upgrading in."_s;
        const QString none = u"There's no system keyring running."_s;
        QTest::newRow("denied by you") << int(QKeychain::AccessDeniedByUser) << u"Cancelled"_s << denied;
        QTest::newRow("denied") << int(QKeychain::AccessDenied) << QString() << denied;
        QTest::newRow("no keyring") << int(QKeychain::NoBackendAvailable) << u"No bus"_s << none;
        QTest::newRow("not implemented") << int(QKeychain::NotImplemented) << QString() << none;
        QTest::newRow("couldn't delete") << int(QKeychain::CouldNotDeleteEntry) << u"It's locked"_s
                                         << u"The system keyring failed: It's locked"_s;
        QTest::newRow("other") << int(QKeychain::OtherError) << u"Something broke"_s
                               << u"The system keyring failed: Something broke"_s;
        QTest::newRow("other, unsaid")
            << int(QKeychain::OtherError) << QString() << u"The system keyring failed."_s;
    }

    void failed()
    {
        QFETCH(int, error);
        QFETCH(QString, why);
        QFETCH(QString, expected);
        const SecretStore::Result result = KeychainSecretStore::resultOf(QKeychain::Error(error), why);
        QCOMPARE(result.status, SecretStore::Status::Failed);
        QCOMPARE(result.error, expected);
    }
};

QTEST_GUILESS_MAIN(TestKeychainSecretStore)
#include "tst_keychainsecretstore.moc"
