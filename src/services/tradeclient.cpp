// SPDX-License-Identifier: MIT
#include "services/tradeclient.h"

#include "core/tradesite.h"
#include "services/network.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <algorithm>
#include <memory>
#include <utility>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace pou {
namespace {

// How long a request may wait its turn. It fails as RateLimited rather than
// keep you waiting any longer.
constexpr std::chrono::seconds maxWait = 30s;

// "1 second", "45 seconds", or in minutes from two on, rounded up so that
// trying again then works.
QString duration(std::chrono::seconds time)
{
    const qint64 seconds = time.count();
    if (seconds == 1)
        return u"1 second"_s;
    if (seconds < 120)
        return u"%1 seconds"_s.arg(seconds);
    return u"%1 minutes"_s.arg((seconds + 59) / 60);
}

QString tooMany(std::chrono::seconds retryAfter)
{
    return u"You've sent the trade site too many requests. Try again in %1."_s.arg(duration(retryAfter));
}

// How long the site is left alone after its nth failure in a row: the first
// rest, doubled each time up to a minute.
std::chrono::milliseconds restAfter(int failures, std::chrono::milliseconds first)
{
    return std::min<std::chrono::milliseconds>(60s, first * (1 << std::min(failures - 1, 4)));
}

QString resting(std::chrono::seconds retryAfter)
{
    return u"The trade site isn't answering, so the app is giving it a rest. Try again in %1."_s.arg(
        duration(retryAfter));
}

QString unreachable(const QNetworkReply &reply)
{
    if (reply.error() == QNetworkReply::OperationCanceledError ||
        reply.error() == QNetworkReply::TimeoutError)
        return u"The trade site didn't answer in time."_s;
    return u"Couldn't reach the trade site: %1"_s.arg(reply.errorString());
}

// What an answer said about the site's limits.
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
    };
}

} // namespace

TradeReply::TradeReply(QObject *parent)
    : QObject(parent)
{}

void TradeReply::succeed(const QJsonObject &answer)
{
    m_answer = answer;
    finish();
}

void TradeReply::fail(Error error, const QString &why, std::chrono::seconds retryAfter)
{
    m_error = error;
    m_errorString = why;
    m_retryAfter = retryAfter;
    finish();
}

void TradeReply::finish()
{
    m_finished = true;
    // so whoever made the request can connect to it first, and delete it or
    // the client when it's done
    QMetaObject::invokeMethod(this, &TradeReply::finished, Qt::QueuedConnection);
}

void TradeReply::hold(std::chrono::milliseconds wait)
{
    QMetaObject::invokeMethod(this, [this, wait] { Q_EMIT waiting(wait); }, Qt::QueuedConnection);
}

TradeClient::TradeClient(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
{
    for (Queue *queue : {&m_searches, &m_fetches, &m_data}) {
        queue->timer.setSingleShot(true);
        queue->timer.setTimerType(Qt::PreciseTimer);
        connect(&queue->timer, &QTimer::timeout, this, [this, queue] { pump(*queue); });
    }
}

TradeReply *TradeClient::search(const QString &league, const QJsonObject &body)
{
    QNetworkRequest request = apiRequest(tradeSearchUrl(league));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    return send(m_searches, request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

TradeReply *TradeClient::fetch(const QStringList &listingIds, const QString &searchId)
{
    return send(m_fetches, apiRequest(tradeFetchUrl(listingIds, searchId)));
}

TradeReply *TradeClient::data(const QString &name)
{
    return send(m_data, apiRequest(tradeDataUrl(name)));
}

void TradeClient::setBlocked(const QString &why)
{
    m_blocked = why;
    if (why.isEmpty())
        return;
    for (Queue *queue : {&m_searches, &m_fetches, &m_data}) {
        queue->timer.stop();
        const QList<Queue::Waiting> waiting = std::exchange(queue->waiting, {});
        for (const Queue::Waiting &next : waiting) {
            if (next.trade)
                next.trade->fail(TradeReply::Error::Unavailable, why);
        }
    }
}

bool TradeClient::isSessionId(QStringView text)
{
    return !text.isEmpty() && std::all_of(text.begin(), text.end(),
                                          [](QChar c) { return c.unicode() < 0x80 && c.isLetterOrNumber(); });
}

void TradeClient::setSessionId(const QString &id)
{
    const QString trimmed = id.trimmed();
    m_sessionId = isSessionId(trimmed) ? trimmed.toLatin1() : QByteArray();
}

TradeReply *TradeClient::send(Queue &queue, const QNetworkRequest &request, const QByteArray &body)
{
    auto *trade = new TradeReply(this);
    if (!m_blocked.isEmpty()) {
        trade->fail(TradeReply::Error::Unavailable, m_blocked);
        return trade;
    }
    queue.waiting << Queue::Waiting{trade, request, body, Clock::now() + maxWait};
    pump(queue);
    return trade;
}

void TradeClient::pump(Queue &queue)
{
    while (!queue.waiting.isEmpty()) {
        const Queue::Waiting &next = queue.waiting.constFirst();
        if (!next.trade) {
            queue.waiting.removeFirst();
            continue;
        }
        const Clock::time_point now = Clock::now();
        // one at a time until the site has said its limits, as the ones we
        // guessed may be more than it gives
        if (!queue.heardFrom && queue.inFlight > 0) {
            if (now >= next.deadline) {
                queue.waiting.takeFirst().trade->fail(TradeReply::Error::Network,
                                                      u"The trade site didn't answer in time."_s);
                continue;
            }
            queue.timer.start(std::chrono::ceil<std::chrono::milliseconds>(next.deadline - now));
            return;
        }
        const std::chrono::milliseconds limited = queue.limiter.wait(now);
        const std::chrono::milliseconds rest =
            std::chrono::ceil<std::chrono::milliseconds>(std::max(m_restUntil, now) - now);
        const std::chrono::milliseconds wait = std::max(limited, rest);
        if (wait == 0ms) {
            queue.limiter.sent(now);
            start(queue, queue.waiting.takeFirst());
        } else if (now + wait > next.deadline) {
            const auto retryAfter = std::chrono::ceil<std::chrono::seconds>(wait);
            if (rest >= limited)
                queue.waiting.takeFirst().trade->fail(TradeReply::Error::Network, resting(retryAfter),
                                                      retryAfter);
            else
                queue.waiting.takeFirst().trade->fail(TradeReply::Error::RateLimited, tooMany(retryAfter),
                                                      retryAfter);
        } else {
            next.trade->hold(wait);
            queue.timer.start(wait);
            return;
        }
    }
}

void TradeClient::start(Queue &queue, const Queue::Waiting &waiting)
{
    QNetworkRequest request = waiting.request;
    if (!m_sessionId.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("Cookie"), QByteArrayLiteral("POESESSID=") + m_sessionId);
        // or the network's cookie jar sends its own in place of it
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
        // nor keeps one the site sends back, which would still go out once
        // you've signed out
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
        // and a redirect to another site doesn't get it
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::SameOriginRedirectPolicy);
    }
    TradeReply *trade = waiting.trade;
    QNetworkReply *reply =
        waiting.body.isEmpty() ? m_network->get(request) : m_network->post(request, waiting.body);
    // deleting the trade reply calls the request off
    reply->setParent(trade);
    // counted off once, when it's answered or when it's called off
    ++queue.inFlight;
    const auto counted = std::make_shared<bool>(false);
    const auto settle = [this, &queue, counted] {
        if (std::exchange(*counted, true))
            return;
        --queue.inFlight;
        pump(queue);
    };
    connect(reply, &QNetworkReply::finished, trade, [this, &queue, trade, reply, settle] {
        answered(queue, trade, reply);
        settle();
    });
    connect(reply, &QObject::destroyed, this, settle);
}

void TradeClient::answered(Queue &queue, TradeReply *trade, QNetworkReply *reply)
{
    using Error = TradeReply::Error;
    reply->deleteLater();
    const Clock::time_point now = Clock::now();
    queue.limiter.learn(rateHeaders(*reply), now);
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
    const QString message = json.object().value(u"error").toObject().value(u"message").toString();
    trade->m_siteMessage = message;
    // whatever it said, it's there, and has given its limits if it has any
    if (status != 0)
        queue.heardFrom = true;

    if (status == 0) {
        trade->fail(Error::Network, unreachable(*reply));
    } else if (status == 429 || message.contains(u"rate limit", Qt::CaseInsensitive)) {
        // the site says how long, but a minute if it doesn't
        const int seconds = reply->rawHeader("Retry-After").toInt();
        const std::chrono::seconds retryAfter(seconds > 0 ? seconds : 60);
        // and the rest wait as long
        queue.limiter.holdFor(retryAfter, now);
        trade->fail(Error::RateLimited, tooMany(retryAfter), retryAfter);
    } else if (message.contains(u"too complex", Qt::CaseInsensitive)) {
        trade->fail(Error::TooComplex, u"The trade site says the search has too many filters."_s);
    } else if (!message.isEmpty()) {
        trade->fail(Error::Rejected, u"The trade site refused the search: %1"_s.arg(message));
    } else if (status != 200 || !json.isObject()) {
        trade->fail(Error::BadAnswer, u"The trade site's answer couldn't be read (HTTP %1)."_s.arg(status));
    } else {
        trade->succeed(json.object());
    }
    // a site that isn't answering is left alone for longer each time
    if (trade->error() == Error::Network || trade->error() == Error::BadAnswer)
        m_restUntil = now + restAfter(++m_failures, m_rest);
    else
        m_failures = 0;
}

} // namespace pou
