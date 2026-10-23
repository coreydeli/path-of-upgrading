// SPDX-License-Identifier: MIT
#pragma once

#include "core/ratelimit.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <chrono>

class QNetworkAccessManager;
class QNetworkReply;

namespace pou {

// A request to the trade site. It belongs to the client, and is yours to
// delete once it has finished, even in a slot connected to finished(), or
// before then to call it off.
class TradeReply : public QObject
{
    Q_OBJECT

public:
    enum class Error {
        None,
        RateLimited, // too many requests, see retryAfter()
        TooComplex,  // a search with more filters than the site takes
        Rejected,    // the site said no, see errorString()
        Network,     // the site couldn't be reached
        BadAnswer,   // the site's answer couldn't be read
    };
    Q_ENUM(Error)

    bool isFinished() const { return m_finished; }
    Error error() const { return m_error; }
    // What went wrong, for people.
    QString errorString() const { return m_errorString; }
    // What the site said when it turned the request down, in its words.
    QString siteMessage() const { return m_siteMessage; }
    // How long until the site takes requests again, when RateLimited.
    std::chrono::seconds retryAfter() const { return m_retryAfter; }
    // The site's answer, when there's no error.
    QJsonObject answer() const { return m_answer; }

Q_SIGNALS:
    // Always from the event loop, never from inside a call to the client.
    void finished();
    // When it's next to go but has to wait its turn under the site's limits,
    // with how long that will be. It can come again with the time left, as
    // when the site answers another request. Also from the event loop.
    void waiting(std::chrono::milliseconds wait);

private:
    friend class TradeClient;

    explicit TradeReply(QObject *parent);
    void succeed(const QJsonObject &answer);
    void fail(Error error, const QString &why, std::chrono::seconds retryAfter = {});
    void finish();
    void hold(std::chrono::milliseconds wait);

    bool m_finished = false;
    Error m_error = Error::None;
    QString m_errorString;
    QString m_siteMessage;
    std::chrono::seconds m_retryAfter{};
    QJsonObject m_answer;
};

// The official trade site. Searches, fetches and data requests each wait
// their turn to stay under the site's limits, and fail as RateLimited when
// that would take more than half a minute.
class TradeClient : public QObject
{
    Q_OBJECT

public:
    // The network has to outlive the client.
    explicit TradeClient(QNetworkAccessManager *network, QObject *parent = nullptr);

    // A TradeQuery's body. The answer has the search's "id", how many
    // listings it found in "total" and the ids of the first 100 in "result".
    TradeReply *search(const QString &league, const QJsonObject &body);
    // Up to tradeFetchLimit of the listings a search found, for
    // listingsFromJson().
    TradeReply *fetch(const QStringList &listingIds, const QString &searchId);
    // "stats", "filters" or "leagues", see tradeDataUrl().
    TradeReply *data(const QString &name);

    // Whether the text could be your session id on the site: the POESESSID
    // cookie of a browser that's signed in to it, which is letters and digits.
    static bool isSessionId(QStringView text);
    // Sends requests signed in with that session id, once it's trimmed, or
    // signed out when it isn't one.
    void setSessionId(const QString &id);

private:
    using Clock = RateLimiter::Clock;

    // Requests the site limits together.
    struct Queue
    {
        struct Waiting
        {
            QPointer<TradeReply> trade; // null once it's called off
            QNetworkRequest request;
            QByteArray body;
            Clock::time_point deadline;
        };

        RateLimiter limiter;
        QList<Waiting> waiting; // in the order they were made
        QTimer timer;           // for when the first can go
    };

    // POSTed when there's a body
    TradeReply *send(Queue &queue, const QNetworkRequest &request, const QByteArray &body = {});
    // Sends what can go, and fails what would wait too long.
    void pump(Queue &queue);
    void start(Queue &queue, const Queue::Waiting &waiting);
    void answered(Queue &queue, TradeReply *trade, QNetworkReply *reply);

    QNetworkAccessManager *m_network;
    QByteArray m_sessionId;
    Queue m_searches;
    Queue m_fetches;
    Queue m_data;
};

} // namespace pou
