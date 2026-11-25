// SPDX-License-Identifier: MIT
#pragma once

#include "core/poeapi.h"
#include "core/poeoauth.h"
#include "core/ratelimit.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace pou {

// Where the client gets the access token for the linked account.
class PoeTokenSource
{
public:
    // The new token, or why there isn't one.
    using Handler = std::function<void(const QString &token, const QString &error)>;

    virtual ~PoeTokenSource() = default;

    // Calls back later, with a token that's good for a little while.
    virtual void withToken(const QObject *context, Handler handler) = 0;
    // The website refused the last token, so the next one asked for is a new one.
    virtual void rejectToken() = 0;
};

// What came back for a request.
struct PoeResponse
{
    bool ok = false;
    QByteArray body; // for ok
    PoeFailure failure;
};

// Asks Path of Exile's account API things on behalf of the linked account,
// one request at a time and spaced out by the limits the API gives in its
// answers. A token the website refuses is replaced once; no request is ever
// repeated otherwise, as too many refused ones are penalised.
class PoeClient : public QObject
{
    Q_OBJECT

public:
    using Done = std::function<void(const PoeResponse &)>;

    // They have to outlive it.
    PoeClient(QNetworkAccessManager *network, PoeConfig config, PoeTokenSource *tokens,
              QObject *parent = nullptr);

    // Calls done once, later, unless the context is gone.
    void get(const QUrl &url, const QObject *context, Done done);
    void characters(const QObject *context, Done done) { get(charactersUrl(), context, std::move(done)); }
    void character(const QString &name, const QObject *context, Done done)
    {
        get(characterUrl(name), context, std::move(done));
    }

    // Whether anything is waiting or out.
    bool isBusy() const { return m_active || !m_queue.isEmpty(); }

private:
    struct Job
    {
        QUrl url;
        QPointer<const QObject> context;
        Done done;
    };

    void next();
    void send(const QString &token, bool retried);
    void finish(const PoeResponse &response);
    void fetchToken(bool retried);

    QNetworkAccessManager *m_network;
    PoeConfig m_config;
    PoeTokenSource *m_tokens;
    RateLimiter m_limiter;
    QList<Job> m_queue;
    bool m_active = false;
    Job m_job;
};

} // namespace pou
