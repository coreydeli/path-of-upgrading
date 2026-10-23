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

void TradeSearch::fail(Error error, const QString &why)
{
    m_error = error;
    m_errorString = why;
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

TradeSearch *TradeSearcher::search(const QJsonObject &body)
{
    auto *search = new TradeSearch(this);
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
    TradeReply *reply = search->m_reply;
    reply->deleteLater();

    if (reply->error() != TradeReply::Error::None) {
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
