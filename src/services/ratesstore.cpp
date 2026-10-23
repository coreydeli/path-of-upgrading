// SPDX-License-Identifier: MIT
#include "services/ratesstore.h"

#include "services/logging.h"
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
        return u"poe.ninja didn't answer in time."_s;
    return u"Couldn't reach poe.ninja: %1"_s.arg(reply.errorString());
}

} // namespace

RatesStore::RatesStore(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
{}

RatesStore::~RatesStore()
{
    // calls off what's still asked for
    delete m_reply;
}

double RatesStore::divineRate() const
{
    return m_rates.value(u"divine"_s);
}

void RatesStore::refresh(const QString &league)
{
    if (league != m_league) {
        // what's asked for another league is no use now
        delete m_reply;
        m_league = league;
        m_rates.clear();
        m_fetched = {};
        m_errorString.clear();
    } else if (isRefreshing()) {
        // that one's done when poe.ninja answers
        return;
    }
    m_reply = m_network->get(apiRequest(ninjaCurrencyUrl(m_league)));
    connect(m_reply, &QNetworkReply::finished, this, &RatesStore::answered);
}

// Takes the overview's rates, unless it has no Divine Orb price, which a
// budget in divines can't do without.
bool RatesStore::take(const QJsonObject &overview)
{
    const std::optional<double> divine = divineInExalts(overview);
    if (!divine)
        return false;
    m_rates = currencyRatesInExalts(overview);
    // so listings priced in divines go by the rate you're shown
    m_rates.insert(u"divine"_s, *divine);
    return true;
}

void RatesStore::answered()
{
    QNetworkReply *reply = m_reply;
    reply->deleteLater();
    m_reply.clear();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
    if (status == 0) {
        fail(unreachable(*reply));
    } else if (status != 200 || !json.isObject()) {
        fail(u"poe.ninja's answer couldn't be read (HTTP %1)."_s.arg(status));
    } else if (!take(json.object())) {
        fail(u"poe.ninja has no Divine Orb price for %1."_s.arg(m_league));
    } else {
        m_fetched = QDateTime::currentDateTimeUtc();
        m_errorString.clear();
        qCDebug(lcNinja, "A Divine Orb is worth %.1f Exalted Orbs in %s", divineRate(),
                qUtf8Printable(m_league));
    }
    Q_EMIT refreshed();
}

void RatesStore::fail(const QString &why)
{
    qCWarning(lcNinja, "Couldn't get currency rates for %s from poe.ninja: %s", qUtf8Printable(m_league),
              qUtf8Printable(why));
    m_errorString = why;
}

} // namespace pou
