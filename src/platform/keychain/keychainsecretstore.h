// SPDX-License-Identifier: MIT
#pragma once

#include "platform/secretstore.h"

#include <qt6keychain/keychain.h>

namespace pou {

// Through Qt Keychain, which uses the Secret Service or KWallet, whichever
// the desktop runs. When there's none the secret isn't kept at all, rather
// than in a file.
class KeychainSecretStore : public SecretStore
{
    Q_OBJECT

public:
    // Everything is kept under the service, by the names it's asked for.
    explicit KeychainSecretStore(const QString &service, QObject *parent = nullptr);

    void read(const QString &name, const QObject *context, Done done) override;
    void write(const QString &name, const QString &secret, const QObject *context, Done done) override;
    void remove(const QString &name, const QObject *context, Done done) override;

    // What a job that ended with that error came to, without the secret.
    static Result resultOf(QKeychain::Error error, const QString &errorString);

private:
    void start(QKeychain::Job *job, const QString &name, const QObject *context, Done done);

    QString m_service;
};

} // namespace pou
