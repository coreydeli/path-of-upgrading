// SPDX-License-Identifier: MIT
#include "services/tradesearcher.h"

#include "core/tradequery.h"
#include "core/tradesite.h"
#include "services/logging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// The listing status a search body asks for, as the site names it.
QString statusOf(const QJsonObject &body)
{
    return body.value(u"query").toObject().value(u"status").toObject().value(u"option").toString();
}

QJsonObject withStatus(QJsonObject body, const QString &status)
{
    QJsonObject query = body.value(u"query").toObject();
    query.insert(u"status"_s, QJsonObject{{u"option"_s, status}});
    body.insert(u"query"_s, query);
    return body;
}

} // namespace

TradeSearch::TradeSearch(QObject *parent)
    : QObject(parent)
{}

TradeSearch::~TradeSearch()
{
    delete m_reply;
}

QUrl TradeSearch::url() const
{
    if (m_error == Error::Failed)
        return tradePrefilledUrl(m_league, m_body);
    if (m_error == Error::None && m_finished)
        return tradeResultsUrl(m_league, m_id);
    return {};
}

void TradeSearch::fail(Error error, const QString &why, std::chrono::seconds retryAfter)
{
    m_error = error;
    m_errorString = why;
    m_retryAfter = retryAfter;
    finish();
}

void TradeSearch::finish()
{
    m_finished = true;
    QMetaObject::invokeMethod(this, &TradeSearch::finished, Qt::QueuedConnection);
}

TradeFetch::TradeFetch(TradeClient *client, const QStringList &listingIds, const QString &searchId,
                       QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_listingIds(listingIds)
    , m_searchId(searchId)
{
    next();
}

TradeFetch::~TradeFetch()
{
    delete m_reply;
}

QJsonObject TradeFetch::answer() const
{
    return {{u"result"_s, m_results}};
}

void TradeFetch::next()
{
    const QStringList ids = m_listingIds.mid(m_fetched, tradeFetchLimit);
    if (ids.isEmpty()) {
        finish();
        return;
    }
    m_reply = m_client->fetch(ids, m_searchId);
    connect(m_reply, &TradeReply::waiting, this, &TradeFetch::waiting);
    connect(m_reply, &TradeReply::finished, this, &TradeFetch::answered);
}

void TradeFetch::answered()
{
    TradeReply *reply = m_reply;
    reply->deleteLater();

    if (reply->error() != TradeReply::Error::None) {
        qCWarning(lcTrade, "Couldn't fetch listings from the trade site: %s",
                  qUtf8Printable(reply->errorString()));
        m_error = reply->error();
        m_errorString = reply->errorString();
        m_retryAfter = reply->retryAfter();
        finish();
        return;
    }
    // sold ones come back null, and stay in for listingsFromJson() to leave out
    for (const QJsonValue &result : reply->answer().value(u"result").toArray())
        m_results << result;
    m_fetched = std::min(m_fetched + tradeFetchLimit, int(m_listingIds.size()));
    next();
}

void TradeFetch::finish()
{
    m_finished = true;
    QMetaObject::invokeMethod(this, &TradeFetch::finished, Qt::QueuedConnection);
}

TradeSearcher::TradeSearcher(TradeClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{}

void TradeSearcher::setSettings(const TradeSettings &settings)
{
    m_settings = settings;
}

void TradeSearcher::setLeagues(const QStringList &leagues)
{
    m_leagues = leagues;
}

League TradeSearcher::league() const
{
    return resolveLeague(m_settings.league, m_leagues);
}

QString TradeSearcher::unavailable() const
{
    if (!m_settings.enabled)
        return u"Trade searches are turned off in the settings."_s;
    if (!league().canTrade())
        return u"There's no trading in solo self-found, so trade searches are off."_s;
    return {};
}

TradeSearch *TradeSearcher::search(const QJsonObject &body)
{
    auto *search = new TradeSearch(this);
    if (const QString why = unavailable(); !why.isEmpty()) {
        search->fail(TradeSearch::Error::Unavailable, why);
        return search;
    }
    search->m_league = league().tradeId();
    send(search, body);
    return search;
}

TradeFetch *TradeSearcher::fetch(const QStringList &listingIds, const QString &searchId)
{
    return new TradeFetch(m_client, listingIds, searchId, this);
}

void TradeSearcher::send(TradeSearch *search, const QJsonObject &body)
{
    search->m_body = m_turnedDown.contains(statusOf(body))
                         ? withStatus(body, listingStatusId(ListingStatus::Online))
                         : body;
    qCDebug(lcTrade, "Searching %s: %s", qUtf8Printable(search->m_league),
            QJsonDocument(search->m_body).toJson(QJsonDocument::Compact).constData());
    search->m_reply = m_client->search(search->m_league, search->m_body);
    connect(search->m_reply, &TradeReply::waiting, search, &TradeSearch::waiting);
    connect(search->m_reply, &TradeReply::finished, search, [this, search] { answered(search); });
}

void TradeSearcher::answered(TradeSearch *search)
{
    using Error = TradeReply::Error;
    TradeReply *reply = search->m_reply;
    reply->deleteLater();

    switch (reply->error()) {
    case Error::None:
        break;
    case Error::RateLimited:
        search->fail(TradeSearch::Error::RateLimited, reply->errorString(), reply->retryAfter());
        return;
    case Error::TooComplex:
        search->fail(TradeSearch::Error::TooComplex, reply->errorString());
        return;
    case Error::Rejected:
        // turned down for its listing status, it may go through as online listings
        if (const QString status = statusOf(search->m_body);
            reply->siteMessage().contains(u"status", Qt::CaseInsensitive) && !status.isEmpty() &&
            status != listingStatusId(ListingStatus::Online)) {
            qCInfo(lcTrade, "The trade site turned down the listing status \"%s\", so searching online ones",
                   qUtf8Printable(status));
            m_turnedDown.insert(status);
            send(search, search->m_body);
            return;
        }
        [[fallthrough]];
    case Error::Network:
    case Error::BadAnswer:
        qCWarning(lcTrade, "Couldn't search the trade site: %s", qUtf8Printable(reply->errorString()));
        search->fail(TradeSearch::Error::Failed, reply->errorString());
        return;
    }

    const QJsonObject answer = reply->answer();
    search->m_id = answer.value(u"id").toString();
    if (search->m_id.isEmpty()) {
        qCWarning(lcTrade, "The trade site's answer to a search had no id");
        search->fail(TradeSearch::Error::Failed, u"The trade site's answer couldn't be read."_s);
        return;
    }
    search->m_total = answer.value(u"total").toInt();
    for (const QJsonValue &id : answer.value(u"result").toArray()) {
        if (id.isString())
            search->m_listingIds << id.toString();
    }
    search->finish();
}

} // namespace pou
