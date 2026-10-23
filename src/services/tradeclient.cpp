// SPDX-License-Identifier: MIT
#include "services/tradeclient.h"

#include "core/tradesite.h"
#include "services/network.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QString unreachable(const QNetworkReply &reply)
{
    if (reply.error() == QNetworkReply::OperationCanceledError ||
        reply.error() == QNetworkReply::TimeoutError)
        return u"The trade site didn't answer in time."_s;
    return u"Couldn't reach the trade site: %1"_s.arg(reply.errorString());
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

void TradeReply::fail(Error error, const QString &why)
{
    m_error = error;
    m_errorString = why;
    finish();
}

void TradeReply::finish()
{
    m_finished = true;
    // so whoever made the request can connect to it first, and delete it or
    // the client when it's done
    QMetaObject::invokeMethod(this, &TradeReply::finished, Qt::QueuedConnection);
}

TradeClient::TradeClient(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
{}

TradeReply *TradeClient::search(const QString &league, const QJsonObject &body)
{
    QNetworkRequest request = apiRequest(tradeSearchUrl(league));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    return send(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

TradeReply *TradeClient::fetch(const QStringList &listingIds, const QString &searchId)
{
    return send(apiRequest(tradeFetchUrl(listingIds, searchId)));
}

TradeReply *TradeClient::data(const QString &name)
{
    return send(apiRequest(tradeDataUrl(name)));
}

TradeReply *TradeClient::send(const QNetworkRequest &request, const QByteArray &body)
{
    auto *trade = new TradeReply(this);
    QNetworkReply *reply = body.isEmpty() ? m_network->get(request) : m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, trade, [this, trade, reply] { answered(trade, reply); });
    return trade;
}

void TradeClient::answered(TradeReply *trade, QNetworkReply *reply)
{
    using Error = TradeReply::Error;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument json = QJsonDocument::fromJson(reply->readAll());

    if (status == 0) {
        trade->fail(Error::Network, unreachable(*reply));
    } else if (status != 200 || !json.isObject()) {
        trade->fail(Error::BadAnswer, u"The trade site's answer couldn't be read (HTTP %1)."_s.arg(status));
    } else {
        trade->succeed(json.object());
    }
}

} // namespace pou
