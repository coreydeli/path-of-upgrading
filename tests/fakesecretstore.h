// SPDX-License-Identifier: MIT
#pragma once

#include "platform/secretstore.h"

#include <QHash>
#include <QTimer>

// A keyring for tests that keeps its secrets in memory, and never the real
// one. It answers a moment after it's asked, as the real one does.
class FakeSecretStore : public pou::SecretStore
{
public:
    using pou::SecretStore::SecretStore;

    void read(const QString &name, const QObject *context, Done done) override
    {
        ++reads;
        if (!failure.isEmpty())
            return answer(context, std::move(done), {.status = Status::Failed, .error = failure});
        if (!secrets.contains(name))
            return answer(context, std::move(done), {.status = Status::NotFound});
        answer(context, std::move(done), {.secret = secrets.value(name)});
    }

    void write(const QString &name, const QString &secret, const QObject *context, Done done) override
    {
        ++writes;
        if (!failure.isEmpty())
            return answer(context, std::move(done), {.status = Status::Failed, .error = failure});
        secrets.insert(name, secret);
        answer(context, std::move(done), {});
    }

    void remove(const QString &name, const QObject *context, Done done) override
    {
        ++removals;
        if (!failure.isEmpty())
            return answer(context, std::move(done), {.status = Status::Failed, .error = failure});
        const bool had = secrets.remove(name);
        answer(context, std::move(done), {.status = had ? Status::Done : Status::NotFound});
    }

    QHash<QString, QString> secrets;
    // Why every call fails, when it's set, as when the keyring is locked.
    QString failure;
    int reads = 0;
    int writes = 0;
    int removals = 0;

private:
    static void answer(const QObject *context, Done done, const Result &result)
    {
        QTimer::singleShot(0, context, [done = std::move(done), result] { done(result); });
    }
};
