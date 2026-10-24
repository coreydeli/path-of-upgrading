// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace pou {

// Downloads the code behind a link to a build shared on one of the sites
// Path of Building shares builds on, see buildCodeUrl().
class BuildFetcher : public QObject
{
    Q_OBJECT

public:
    // The network has to outlive it.
    explicit BuildFetcher(QNetworkAccessManager *network, QObject *parent = nullptr);
    ~BuildFetcher() override;

    // In place of the one before. False when it isn't a link to one of the
    // sites, and then nothing is asked for.
    bool fetch(const QString &link);
    bool isFetching() const { return !m_reply.isNull(); }

Q_SIGNALS:
    void fetched(const QString &code);
    void failed(const QString &why);

private:
    void answered();

    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
};

} // namespace pou
