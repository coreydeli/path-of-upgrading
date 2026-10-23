// SPDX-License-Identifier: MIT
#include "services/tradesearcher.h"

#include "core/tradesite.h"
#include "services/logging.h"

#include <QJsonArray>
#include <QJsonDocument>

using namespace Qt::Literals::StringLiterals;

namespace pou {

TradeSearch::TradeSearch(QObject *parent)
    : QObject(parent)
{}

TradeSearch::~TradeSearch()
{
    delete m_reply;
}

QUrl TradeSearch::url() const
{
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

void TradeSearcher::send(TradeSearch *search, const QJsonObject &body)
{
    qCDebug(lcTrade, "Searching %s: %s", qUtf8Printable(search->m_league),
            QJsonDocument(body).toJson(QJsonDocument::Compact).constData());
    search->m_reply = m_client->search(search->m_league, body);
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
