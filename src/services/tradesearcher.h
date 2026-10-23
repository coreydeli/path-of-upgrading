// SPDX-License-Identifier: MIT
#pragma once

#include "core/league.h"
#include "core/settings.h"
#include "services/tradeclient.h"

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <chrono>

namespace pou {

// A search on the trade site, from TradeSearcher::search(). It's yours to
// delete once it has finished, even in a slot connected to finished(), or
// before then to call it off.
class TradeSearch : public QObject
{
    Q_OBJECT

public:
    enum class Error {
        None,
        Unavailable, // no trade searches, see TradeSearcher::unavailable()
        RateLimited, // too many requests, see retryAfter()
        TooComplex,  // more filters than the site takes, so it has to be smaller
        Failed,      // see errorString()
    };
    Q_ENUM(Error)

    ~TradeSearch() override;

    bool isFinished() const { return m_finished; }
    Error error() const { return m_error; }
    // What went wrong, for people.
    QString errorString() const { return m_errorString; }
    // How long until the site takes searches again, when RateLimited.
    std::chrono::seconds retryAfter() const { return m_retryAfter; }

    // The trade site's id for the league it searches: "HC Runes of Aldur".
    QString league() const { return m_league; }
    // The listings it found on the site, to open in the browser. Empty until
    // it has finished without an error.
    QUrl url() const;

    // What it found, once it has finished without an error: the search's id
    // on the site, how many listings there are, and the ids of the first
    // hundred in the order it sorts them.
    QString id() const { return m_id; }
    int total() const { return m_total; }
    QStringList listingIds() const { return m_listingIds; }

Q_SIGNALS:
    // Always from the event loop.
    void finished();

private:
    friend class TradeSearcher;

    explicit TradeSearch(QObject *parent);
    void fail(Error error, const QString &why, std::chrono::seconds retryAfter = {});
    void finish();

    QPointer<TradeReply> m_reply;
    bool m_finished = false;
    Error m_error = Error::None;
    QString m_errorString;
    std::chrono::seconds m_retryAfter{};
    QString m_league;
    QString m_id;
    int m_total = 0;
    QStringList m_listingIds;
};

// Searches the trade site the way your settings say: in your league, and not
// at all when you've turned searches off or play solo self-found.
class TradeSearcher : public QObject
{
    Q_OBJECT

public:
    // The client has to outlive the searcher.
    explicit TradeSearcher(TradeClient *client, QObject *parent = nullptr);

    void setSettings(const TradeSettings &settings);
    // The site's league ids, from TradeDataStore::leagues().
    void setLeagues(const QStringList &leagues);

    // The league searches go to, see resolveLeague().
    League league() const;
    // Why there are no trade searches, or empty when there are.
    QString unavailable() const;

    // A TradeQuery's body, searched for in league().
    TradeSearch *search(const QJsonObject &body);

private:
    void send(TradeSearch *search, const QJsonObject &body);
    void answered(TradeSearch *search);

    TradeClient *m_client;
    TradeSettings m_settings;
    QStringList m_leagues;
};

} // namespace pou
