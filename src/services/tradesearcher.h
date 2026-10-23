// SPDX-License-Identifier: MIT
#pragma once

#include "core/league.h"
#include "core/settings.h"
#include "services/tradeclient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSet>
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
        Failed,      // see errorString(), and url() to try it in the browser
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
    // The search on the site, to open in the browser: the listings it found,
    // or when it failed, the site's search page filled in with it, which may
    // still work there. Empty before it has finished and for other errors.
    QUrl url() const;

    // What it found, once it has finished without an error: the search's id
    // on the site, how many listings there are, and the ids of the first
    // hundred in the order it sorts them, for TradeSearcher::fetch().
    QString id() const { return m_id; }
    int total() const { return m_total; }
    QStringList listingIds() const { return m_listingIds; }

Q_SIGNALS:
    // Always from the event loop.
    void finished();
    // While it waits its turn, as TradeReply::waiting().
    void waiting(std::chrono::milliseconds wait);

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
    QJsonObject m_body; // as it was sent
    QString m_id;
    int m_total = 0;
    QStringList m_listingIds;
};

// Listings a search found, from TradeSearcher::fetch(). It's yours to delete
// as a TradeSearch is.
class TradeFetch : public QObject
{
    Q_OBJECT

public:
    ~TradeFetch() override;

    bool isFinished() const { return m_finished; }
    TradeReply::Error error() const { return m_error; }
    QString errorString() const { return m_errorString; }
    std::chrono::seconds retryAfter() const { return m_retryAfter; }

    // The listings, in the order they were asked for and shaped like the
    // site's answer, for listingsFromJson().
    QJsonObject answer() const;
    // How many of the ids the site answered for, from the first. Fewer than
    // were asked for when there's an error, as it stops there.
    int fetched() const { return m_fetched; }

Q_SIGNALS:
    // Always from the event loop.
    void finished();
    // While it waits its turn, as TradeReply::waiting().
    void waiting(std::chrono::milliseconds wait);

private:
    friend class TradeSearcher;

    TradeFetch(TradeClient *client, const QStringList &listingIds, const QString &searchId, QObject *parent);
    void next();
    void answered();
    void finish();

    TradeClient *m_client;
    QStringList m_listingIds;
    QString m_searchId;
    QPointer<TradeReply> m_reply;
    bool m_finished = false;
    TradeReply::Error m_error = TradeReply::Error::None;
    QString m_errorString;
    std::chrono::seconds m_retryAfter{};
    QJsonArray m_results;
    int m_fetched = 0;
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

    // A TradeQuery's body, searched for in league(). When the site turns
    // down the listing status it asks for, it's searched again for online
    // listings, as that status is from then on.
    TradeSearch *search(const QJsonObject &body);
    // Listings a search found, a few at a time.
    TradeFetch *fetch(const QStringList &listingIds, const QString &searchId);

private:
    void send(TradeSearch *search, const QJsonObject &body);
    void answered(TradeSearch *search);

    TradeClient *m_client;
    TradeSettings m_settings;
    QStringList m_leagues;
    QSet<QString> m_turnedDown; // listing statuses the site wouldn't search for
};

} // namespace pou
