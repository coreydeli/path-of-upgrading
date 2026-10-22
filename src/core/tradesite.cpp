// SPDX-License-Identifier: MIT
#include "core/tradesite.h"

#include <QJsonDocument>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Encoded like the site's own links: everything but letters, digits and
// -._~, so a league name stays one part of the path and a search body reads
// back the same, '+' and '&' included.
QString encoded(const QString &part)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(part));
}

// The path and query are already encoded.
QUrl siteUrl(const QString &path, const QString &query = {})
{
    QUrl url;
    url.setScheme(u"https"_s);
    url.setHost(u"www.pathofexile.com"_s);
    url.setPath(path, QUrl::TolerantMode);
    if (!query.isEmpty())
        url.setQuery(query, QUrl::TolerantMode);
    return url;
}

} // namespace

QUrl tradeDataUrl(const QString &name)
{
    return siteUrl(u"/api/trade2/data/"_s + encoded(name));
}

QUrl tradeSearchUrl(const QString &league)
{
    return siteUrl(u"/api/trade2/search/poe2/"_s + encoded(league));
}

QUrl tradeFetchUrl(const QStringList &listingIds, const QString &searchId)
{
    Q_ASSERT(listingIds.size() <= tradeFetchLimit);
    QStringList ids;
    for (const QString &id : listingIds)
        ids << encoded(id);
    return siteUrl(u"/api/trade2/fetch/"_s + ids.join(u','),
                   u"query="_s + encoded(searchId) + u"&realm=poe2"_s);
}

QUrl tradeResultsUrl(const QString &league, const QString &searchId)
{
    return siteUrl(u"/trade2/search/poe2/"_s + encoded(league) + u'/' + encoded(searchId));
}

QUrl tradePrefilledUrl(const QString &league, const QJsonObject &body)
{
    const QString json = QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact));
    return siteUrl(u"/trade2/search/poe2/"_s + encoded(league), u"q="_s + encoded(json));
}

} // namespace pou
