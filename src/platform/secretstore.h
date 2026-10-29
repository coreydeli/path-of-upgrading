// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <memory>

namespace pou {

// Where the system keeps passwords and the like, its keyring. A secret is
// kept under a name, and every call answers later, from the event loop.
class SecretStore : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        Done,
        NotFound, // nothing is kept under that name
        Failed,   // see error
    };

    struct Result
    {
        Status status = Status::Done;
        QString secret; // what a read found
        QString error;  // why it failed, for people
    };
    using Done = std::function<void(const Result &)>;

    using QObject::QObject;

    // Each calls done once, unless the context is gone by then. Taking out
    // what isn't there is NotFound.
    virtual void read(const QString &name, const QObject *context, Done done) = 0;
    virtual void write(const QString &name, const QString &secret, const QObject *context, Done done) = 0;
    virtual void remove(const QString &name, const QObject *context, Done done) = 0;
};

// The one for this desktop.
std::unique_ptr<SecretStore> createSecretStore();

} // namespace pou
