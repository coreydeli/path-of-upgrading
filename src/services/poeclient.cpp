// SPDX-License-Identifier: MIT
#include "services/poeclient.h"

#include "services/network.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// What an answer said about the limits.
RateHeaders rateHeaders(const QNetworkReply &reply)
{
    const auto header = [&reply](const char *name) {
        return QString::fromLatin1(reply.rawHeader(name));
    };
    return {
        .ip = header("X-Rate-Limit-Ip"),
        .ipState = header("X-Rate-Limit-Ip-State"),
        .account = header("X-Rate-Limit-Account"),
        .accountState = header("X-Rate-Limit-Account-State"),
        .client = header("X-Rate-Limit-Client"),
        .clientState = header("X-Rate-Limit-Client-State"),
    };
}

PoeResponse failed(PoeFailure::Kind kind, const QString &text)
{
    return {.ok = false, .body = {}, .failure = {.kind = kind, .waitSeconds = 0, .text = text}};
}

} // namespace

PoeClient::PoeClient(QNetworkAccessManager *network, PoeConfig config, PoeTokenSource *tokens,
                     QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_config(std::move(config))
    , m_tokens(tokens)
{}

void PoeClient::get(const QUrl &url, const QObject *context, Done done)
{
    m_queue << Job{.url = url, .context = context, .done = std::move(done)};
    if (!m_active)
        next();
}

void PoeClient::next()
{
    if (m_queue.isEmpty()) {
        m_active = false;
        return;
    }
    m_active = true;
    m_job = m_queue.takeFirst();
    // nobody's waiting for it any more
    if (!m_job.context) {
        next();
        return;
    }
    fetchToken(false);
}

void PoeClient::fetchToken(bool retried)
{
    m_tokens->withToken(this, [this, retried](const QString &token, const QString &error) {
        // whoever asked has gone while the token was coming
        if (!m_job.context) {
            finish({});
            return;
        }
        if (token.isEmpty()) {
            finish(failed(PoeFailure::Kind::SignedOut, error));
            return;
        }
        // the limits say when it can go, the token is as new as it can be
        const auto wait = m_limiter.wait(RateLimiter::Clock::now());
        if (wait.count() > 0) {
            QTimer::singleShot(wait, this, [this, token, retried] { send(token, retried); });
            return;
        }
        send(token, retried);
    });
}

void PoeClient::send(const QString &token, bool retried)
{
    QNetworkRequest request = poeRequest(m_job.url, m_config);
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    m_limiter.sent(RateLimiter::Clock::now());
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, retried] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (status == 0) {
            finish(failed(PoeFailure::Kind::Other,
                          u"Couldn't reach the website. Check your connection and try again."_s));
            return;
        }
        m_limiter.learn(rateHeaders(*reply), RateLimiter::Clock::now());
        if (status == 200) {
            finish({.ok = true, .body = body, .failure = {}});
            return;
        }
        const QString retryAfter = QString::fromLatin1(reply->rawHeader("Retry-After"));
        const PoeFailure failure = poeFailure(status, body, retryAfter);
        if (failure.kind == PoeFailure::Kind::RateLimited && failure.waitSeconds > 0)
            m_limiter.holdFor(std::chrono::seconds(failure.waitSeconds), RateLimiter::Clock::now());
        if (failure.kind == PoeFailure::Kind::SignedOut && !retried) {
            // maybe only that token, so the once
            m_tokens->rejectToken();
            fetchToken(true);
            return;
        }
        finish({.ok = false, .body = {}, .failure = failure});
    });
}

void PoeClient::finish(const PoeResponse &response)
{
    Job job = std::exchange(m_job, {});
    // after this one, from the event loop, so a reply isn't deleted in its own slot
    QTimer::singleShot(0, this, [this] { next(); });
    if (job.context)
        job.done(response);
}

} // namespace pou
