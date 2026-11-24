// SPDX-License-Identifier: MIT
#include "services/network.h"

namespace pou {

QByteArray userAgent()
{
    return QByteArrayLiteral("PathOfUpgrading/" APP_VERSION " (+" APP_HOMEPAGE ")");
}

QNetworkRequest apiRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    // long enough for a big search on a slow day
    request.setTransferTimeout(20000);
    return request;
}

QByteArray poeUserAgent(const PoeConfig &config)
{
    return "OAuth " + config.clientId.toUtf8() + "/" APP_VERSION " (contact: " + config.contact.toUtf8() +
           ") (+" APP_HOMEPAGE ")";
}

QNetworkRequest poeRequest(const QUrl &url, const PoeConfig &config)
{
    QNetworkRequest request = apiRequest(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, poeUserAgent(config));
    return request;
}

} // namespace pou
