// SPDX-License-Identifier: MIT
#include "services/updatedownloader.h"

#include "services/logging.h"
#include "services/network.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// The biggest file it will take, as the packages are far smaller
constexpr qint64 mostBytes = qint64(1) << 30;

} // namespace

UpdateDownloader::UpdateDownloader(QNetworkAccessManager *network, const QString &folder, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_root(folder)
{}

UpdateDownloader::~UpdateDownloader()
{
    // calls off what's still coming, and leaves nothing half done
    if (isRunning())
        cancel();
}

void UpdateDownloader::start(const Release &release, InstallKind kind)
{
    if (isRunning())
        return;
    if (m_root.isEmpty()) {
        Q_EMIT failed(u"There's no folder to download the update into."_s);
        return;
    }
    const std::optional<ReleaseAsset> asset = assetFor(release, kind);
    if (!asset) {
        Q_EMIT failed(u"This release has no file for the way you installed the app."_s);
        return;
    }
    const std::optional<ReleaseAsset> sums = checksumsOf(release);
    if (!sums) {
        Q_EMIT failed(u"This release has no checksums, so the download can't be checked."_s);
        return;
    }
    // names with a path in them are dropped when the releases are read, but
    // it's the one thing that mustn't be wrong
    if (QFileInfo(asset->name).fileName() != asset->name || asset->name.startsWith(u'.')) {
        Q_EMIT failed(u"The release's file has a name that can't be used."_s);
        return;
    }
    m_asset = *asset;
    m_sums = *sums;
    m_checksums.clear();
    m_stage = Stage::Sums;
    m_folder = m_root + u'/' + release.version.text();
    clear();
    fetch(m_sums);
}

void UpdateDownloader::cancel()
{
    if (!isRunning())
        return;
    dropReply();
    m_file.close();
    m_stage = Stage::Idle;
    clear();
}

// Stops asking, without its finished coming here.
void UpdateDownloader::dropReply()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply.clear();
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}

// Starts again with an empty folder that's yours alone.
void UpdateDownloader::clear()
{
    // never the current folder
    if (m_root.isEmpty())
        return;
    QDir(m_root).removeRecursively();
    if (m_stage == Stage::Idle)
        return;
    QDir().mkpath(m_folder);
    QFile::setPermissions(m_folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

void UpdateDownloader::fetch(const ReleaseAsset &asset)
{
    QNetworkRequest request(asset.url);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    // the files are sent from another host of GitHub's
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network->get(request);
    if (m_stage == Stage::File) {
        m_received = 0;
        m_hash.reset();
        m_file.setFileName(m_folder + u'/' + m_asset.name);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            const QString why = m_file.errorString();
            delete m_reply;
            fail(u"Couldn't save the update: %1"_s.arg(why));
            return;
        }
        m_file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        connect(m_reply, &QNetworkReply::readyRead, this, &UpdateDownloader::drain);
        connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            Q_EMIT progress(received, total > 0 ? total : m_asset.size);
        });
        connect(m_reply, &QNetworkReply::finished, this, &UpdateDownloader::fileCame);
    } else {
        connect(m_reply, &QNetworkReply::finished, this, &UpdateDownloader::sumsCame);
    }
}

// Writes what has come, and stops at the limit.
void UpdateDownloader::drain()
{
    if (!m_reply || m_stage != Stage::File)
        return;
    const QByteArray chunk = m_reply->readAll();
    m_received += chunk.size();
    if (m_received > mostBytes || m_received > std::max<qint64>(m_asset.size, 0) + 1024 * 1024) {
        dropReply();
        fail(u"The file is bigger than the release says, so it was thrown away."_s);
        return;
    }
    m_hash.addData(chunk);
    if (m_file.write(chunk) != chunk.size()) {
        dropReply();
        fail(u"Couldn't save the update: %1"_s.arg(m_file.errorString()));
        return;
    }
}

void UpdateDownloader::sumsCame()
{
    QNetworkReply *reply = m_reply;
    reply->deleteLater();
    m_reply.clear();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        fail(u"Couldn't reach GitHub: %1"_s.arg(reply->errorString()));
        return;
    }
    if (status != 200) {
        fail(u"GitHub didn't send the checksums (HTTP %1)."_s.arg(status));
        return;
    }
    m_checksums = parseChecksums(reply->readAll());
    if (!m_checksums.contains(m_asset.name)) {
        fail(u"The release's checksums don't list %1, so the download can't be checked."_s.arg(m_asset.name));
        return;
    }
    m_stage = Stage::File;
    fetch(m_asset);
}

void UpdateDownloader::fileCame()
{
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    reply->deleteLater();
    // what came last
    drain();
    if (m_stage != Stage::File)
        return;
    m_reply.clear();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        fail(u"Couldn't download the update: %1"_s.arg(reply->errorString()));
        return;
    }
    if (status != 200) {
        fail(u"GitHub didn't send the file (HTTP %1)."_s.arg(status));
        return;
    }
    m_file.close();
    const QString sum = QString::fromLatin1(m_hash.result().toHex());
    if (sum != m_checksums.value(m_asset.name)) {
        qCWarning(lcUpdates) << "The checksum of" << m_asset.name << "is" << sum;
        fail(u"The download didn't match its checksum, so it was thrown away."_s);
        return;
    }
    m_stage = Stage::Idle;
    Q_EMIT done(m_file.fileName());
}

void UpdateDownloader::fail(const QString &why)
{
    m_file.close();
    m_stage = Stage::Idle;
    clear();
    Q_EMIT failed(why);
}

} // namespace pou
