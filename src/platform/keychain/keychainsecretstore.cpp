// SPDX-License-Identifier: MIT
#include "platform/keychain/keychainsecretstore.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

KeychainSecretStore::KeychainSecretStore(const QString &service, QObject *parent)
    : SecretStore(parent)
    , m_service(service)
{}

void KeychainSecretStore::read(const QString &name, const QObject *context, Done done)
{
    start(new QKeychain::ReadPasswordJob(m_service, this), name, context, std::move(done));
}

void KeychainSecretStore::write(const QString &name, const QString &secret, const QObject *context, Done done)
{
    auto *job = new QKeychain::WritePasswordJob(m_service, this);
    job->setTextData(secret);
    start(job, name, context, std::move(done));
}

void KeychainSecretStore::remove(const QString &name, const QObject *context, Done done)
{
    start(new QKeychain::DeletePasswordJob(m_service, this), name, context, std::move(done));
}

KeychainSecretStore::Result KeychainSecretStore::resultOf(QKeychain::Error error, const QString &errorString)
{
    switch (error) {
    case QKeychain::NoError:
        return {};
    case QKeychain::EntryNotFound:
        return {.status = Status::NotFound};
    case QKeychain::AccessDeniedByUser:
    case QKeychain::AccessDenied:
        return {.status = Status::Failed,
                .error = u"The system keyring wouldn't let Path of Upgrading in."_s};
    case QKeychain::NoBackendAvailable:
    case QKeychain::NotImplemented:
        return {.status = Status::Failed, .error = u"There's no system keyring running."_s};
    case QKeychain::CouldNotDeleteEntry:
    case QKeychain::OtherError:
        break;
    }
    return {.status = Status::Failed,
            .error = errorString.isEmpty() ? u"The system keyring failed."_s
                                           : u"The system keyring failed: %1"_s.arg(errorString)};
}

void KeychainSecretStore::start(QKeychain::Job *job, const QString &name, const QObject *context, Done done)
{
    job->setKey(name);
    // or it would keep the secret in a file when there's no keyring
    job->setInsecureFallback(false);
    // the job deletes itself once it has finished
    connect(job, &QKeychain::Job::finished, context, [done = std::move(done)](QKeychain::Job *finished) {
        Result result = resultOf(finished->error(), finished->errorString());
        if (const auto *read = qobject_cast<QKeychain::ReadPasswordJob *>(finished);
            read && result.status == Status::Done)
            result.secret = read->textData();
        done(result);
    });
    job->start();
}

} // namespace pou
