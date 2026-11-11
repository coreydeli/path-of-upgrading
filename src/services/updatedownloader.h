// SPDX-License-Identifier: MIT
#pragma once

#include "core/updates.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace pou {

// Downloads the file of a release for how the program is installed, and checks
// it against the release's SHA256SUMS before saying it's there. The file goes
// in a folder of its own that only you can open, emptied first. What it
// downloads is untrusted until the checksum matches, and a file that doesn't
// match is deleted.
class UpdateDownloader : public QObject
{
    Q_OBJECT

public:
    // The network has to outlive it. The folder is where downloads go, and
    // everything already in it is deleted when one starts.
    UpdateDownloader(QNetworkAccessManager *network, const QString &folder, QObject *parent = nullptr);
    ~UpdateDownloader() override;

    // Does nothing while it's downloading.
    void start(const Release &release, InstallKind kind);
    // Stops and deletes what it has.
    void cancel();

    bool isRunning() const { return m_stage != Stage::Idle; }
    // The folder the file is in, once it's there.
    QString folder() const { return m_folder; }

Q_SIGNALS:
    // How much has come, and how much there will be (0 when it isn't known).
    void progress(qint64 received, qint64 total);
    // The file is downloaded and its checksum is right.
    void done(const QString &file);
    // Why it isn't, in words.
    void failed(const QString &why);

private:
    enum class Stage { Idle, Sums, File };

    void fetch(const ReleaseAsset &asset);
    void dropReply();
    void drain();
    void sumsCame();
    void fileCame();
    void fail(const QString &why);
    void clear();

    QNetworkAccessManager *m_network;
    QString m_root;
    QString m_folder;
    Stage m_stage = Stage::Idle;
    ReleaseAsset m_asset;
    ReleaseAsset m_sums;
    QHash<QString, QString> m_checksums;
    QPointer<QNetworkReply> m_reply;
    QFile m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    qint64 m_received = 0;
};

} // namespace pou
