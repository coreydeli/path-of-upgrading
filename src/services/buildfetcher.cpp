// SPDX-License-Identifier: MIT
#include "services/buildfetcher.h"

#include "core/pobbuild.h"
#include "services/logging.h"
#include "services/network.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>

using namespace Qt::Literals::StringLiterals;

namespace pou {

BuildFetcher::BuildFetcher(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
{}

BuildFetcher::~BuildFetcher()
{
    delete m_reply;
}

bool BuildFetcher::fetch(const QString &link)
{
    const QUrl url = buildCodeUrl(link);
    if (url.isEmpty())
        return false;
    delete m_reply;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    request.setTransferTimeout(20000);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &BuildFetcher::answered);
    return true;
}

void BuildFetcher::answered()
{
    QNetworkReply *reply = m_reply;
    reply->deleteLater();
    m_reply.clear();

    const QString host = reply->request().url().host();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString code = QString::fromUtf8(reply->readAll()).trimmed();
    QString why;
    if (status == 0) {
        why = reply->error() == QNetworkReply::OperationCanceledError ||
                      reply->error() == QNetworkReply::TimeoutError
                  ? u"%1 didn't answer in time."_s.arg(host)
                  : u"Couldn't reach %1: %2"_s.arg(host, reply->errorString());
    } else if (status == 404 || (status == 200 && code.isEmpty())) {
        why = u"There's no build at that link."_s;
    } else if (status != 200) {
        why = u"%1 couldn't give the build (HTTP %2)."_s.arg(host).arg(status);
    }
    if (!why.isEmpty()) {
        qCInfo(lcPob, "Couldn't download the build from %s: %s",
               qUtf8Printable(reply->request().url().toString()), qUtf8Printable(why));
        Q_EMIT failed(why);
        return;
    }
    Q_EMIT fetched(code);
}

} // namespace pou
