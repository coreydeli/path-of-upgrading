// SPDX-License-Identifier: MIT
#include "services/tradeclient.h"

#include "core/tradesite.h"
#include "services/network.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <algorithm>

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
    return u"The trade site has had too many requests from you. Try again in %1."_s.arg(duration(retryAfter));
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
        const std::chrono::milliseconds wait = queue.limiter.wait(now);
        if (wait == 0ms) {
            queue.limiter.sent(now);
            start(queue, queue.waiting.takeFirst());
        } else if (now + wait > next.deadline) {
            const auto retryAfter = std::chrono::ceil<std::chrono::seconds>(wait);
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
    }
    TradeReply *trade = waiting.trade;
    QNetworkReply *reply =
        waiting.body.isEmpty() ? m_network->get(request) : m_network->post(request, waiting.body);
    // deleting the trade reply calls the request off
    reply->setParent(trade);
    connect(reply, &QNetworkReply::finished, trade,
            [this, &queue, trade, reply] { answered(queue, trade, reply); });
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
        trade->fail(Error::Rejected, u"The trade site says: %1"_s.arg(message));
    } else if (status != 200 || !json.isObject()) {
        trade->fail(Error::BadAnswer, u"The trade site's answer couldn't be read (HTTP %1)."_s.arg(status));
    } else {
        trade->succeed(json.object());
    }
    // what the site said can change when the next can go
    pump(queue);
}

} // namespace pou
