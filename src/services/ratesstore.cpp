// SPDX-License-Identifier: MIT
#include "services/ratesstore.h"

#include "core/datafiles.h"
#include "services/logging.h"
#include "services/network.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <chrono>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace pou {
namespace {

constexpr std::chrono::seconds freshFor = 1h;
constexpr std::chrono::seconds holdFor = 5min;

// Whether that was less than the time ago. A time ahead of the clock counts
// as long ago, or it would never be asked for again.
bool within(const QDateTime &then, const QDateTime &now, std::chrono::seconds time)
{
    const qint64 ago = then.secsTo(now);
    return then.isValid() && ago >= 0 && ago < time.count();
}

QString unreachable(const QNetworkReply &reply)
{
    if (reply.error() == QNetworkReply::OperationCanceledError ||
        reply.error() == QNetworkReply::TimeoutError)
        return u"poe.ninja didn't answer in time."_s;
    return u"Couldn't reach poe.ninja: %1"_s.arg(reply.errorString());
}

} // namespace

RatesStore::RatesStore(QNetworkAccessManager *network, const QString &folder, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_folder(folder)
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

void RatesStore::refresh(const QString &league, bool force)
{
    if (league != m_league) {
        // what's asked for another league is no use now
        delete m_reply;
        m_league = league;
        m_rates.clear();
        m_fetched = {};
        m_failed = {};
        m_errorString.clear();
        read();
    } else if (isRefreshing()) {
        // that one's done when poe.ninja answers
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!force && (within(m_fetched, now, freshFor) || within(m_failed, now, holdFor))) {
        QMetaObject::invokeMethod(this, &RatesStore::refreshed, Qt::QueuedConnection);
        return;
    }
    m_reply = m_network->get(apiRequest(ninjaCurrencyUrl(m_league)));
    connect(m_reply, &QNetworkReply::finished, this, &RatesStore::answered);
}

// The league's file, with its id percent-encoded so nothing in it can lead
// out of the folder.
QString RatesStore::path() const
{
    return m_folder + u'/' + QString::fromLatin1(QUrl::toPercentEncoding(m_league, " ")) + u".json"_s;
}

void RatesStore::read()
{
    const QString file = path();
    if (const std::optional<QJsonObject> overview = readJsonFile(file); overview && take(*overview))
        m_fetched = QFileInfo(file).lastModified().toUTC();
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
    const QByteArray body = reply->readAll();
    const QJsonDocument json = QJsonDocument::fromJson(body);
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
        const QString file = path();
        QString error;
        if (!writeFile(file, body, &error))
            qCWarning(lcNinja, "Couldn't keep poe.ninja's rates for %s in %s: %s", qUtf8Printable(m_league),
                      qUtf8Printable(file), qUtf8Printable(error));
    }
    Q_EMIT refreshed();
}

void RatesStore::fail(const QString &why)
{
    qCWarning(lcNinja, "Couldn't get currency rates for %s from poe.ninja: %s", qUtf8Printable(m_league),
              qUtf8Printable(why));
    m_failed = QDateTime::currentDateTimeUtc();
    m_errorString = why;
}

} // namespace pou
