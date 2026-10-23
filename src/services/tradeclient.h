// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QStringList>
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
    // How long until the site takes requests again, when RateLimited.
    std::chrono::seconds retryAfter() const { return m_retryAfter; }
    // The site's answer, when there's no error.
    QJsonObject answer() const { return m_answer; }

Q_SIGNALS:
    // Always from the event loop, never from inside a call to the client.
    void finished();

private:
    friend class TradeClient;

    explicit TradeReply(QObject *parent);
    void succeed(const QJsonObject &answer);
    void fail(Error error, const QString &why, std::chrono::seconds retryAfter = {});
    void finish();

    bool m_finished = false;
    Error m_error = Error::None;
    QString m_errorString;
    std::chrono::seconds m_retryAfter{};
    QJsonObject m_answer;
};

// The official trade site.
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

private:
    // POSTed when there's a body
    TradeReply *send(const QNetworkRequest &request, const QByteArray &body = {});
    void answered(TradeReply *trade, QNetworkReply *reply);

    QNetworkAccessManager *m_network;
};

} // namespace pou
