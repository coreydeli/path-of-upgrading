// SPDX-License-Identifier: MIT
#include "services/updatechecker.h"

#include "services/logging.h"
#include "services/network.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace pou {
namespace {

const QUrl releasesUrl(u"https://api.github.com/repos/coreydeli/path-of-upgrading/releases?per_page=30"_s);

} // namespace

UpdateChecker::UpdateChecker(QNetworkAccessManager *network, const Version &have, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_have(have)
{
    m_timer.setInterval(24h);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_settings.checkOnLaunch)
            ask();
    });
}

UpdateChecker::~UpdateChecker()
{
    // calls off what's still asked for
    delete m_reply;
}

void UpdateChecker::follow(const UpdateSettings &settings)
{
    if (settings == m_settings)
        return;
    m_settings = settings;
    pick();
}

void UpdateChecker::start()
{
    m_timer.start();
    if (m_settings.checkOnLaunch)
        ask();
}

void UpdateChecker::check()
{
    ask();
}

bool UpdateChecker::isSkipped() const
{
    return m_newest && m_newest->version.text() == m_settings.skipped;
}

void UpdateChecker::ask()
{
    if (isChecking())
        return;
    m_reply = m_network->get(apiRequest(releasesUrl));
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::answered);
    Q_EMIT changed();
}

void UpdateChecker::answered()
{
    QNetworkReply *reply = m_reply;
    reply->deleteLater();
    m_reply.clear();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (status == 0) {
        if (reply->error() == QNetworkReply::OperationCanceledError ||
            reply->error() == QNetworkReply::TimeoutError)
            fail(u"GitHub didn't answer in time."_s);
        else
            fail(u"Couldn't reach GitHub: %1"_s.arg(reply->errorString()));
    } else if (status == 403 || status == 429) {
        fail(u"GitHub says it has been asked too often. Try again in a while."_s);
    } else if (status != 200 || !QJsonDocument::fromJson(body).isArray()) {
        fail(u"GitHub's answer couldn't be read (HTTP %1)."_s.arg(status));
    } else {
        m_releases = readReleases(body);
        m_checked = QDateTime::currentDateTimeUtc();
        m_error.clear();
        pick();
        if (isOffered()) {
            qCDebug(lcUpdates, "%s is out, and this is %s", qUtf8Printable(m_newest->tag),
                    qUtf8Printable(m_have.text()));
            Q_EMIT found(*m_newest);
        }
    }
}

void UpdateChecker::pick()
{
    m_newest = newerRelease(m_releases, m_have, m_settings.includeBetas);
    Q_EMIT changed();
}

void UpdateChecker::fail(const QString &why)
{
    qCWarning(lcUpdates, "Couldn't look for an update: %s", qUtf8Printable(why));
    m_error = why;
    Q_EMIT changed();
}

} // namespace pou
