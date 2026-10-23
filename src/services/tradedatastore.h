// SPDX-License-Identifier: MIT
#pragma once

#include "core/tradequery.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <array>

namespace pou {

class TradeClient;
class TradeReply;

// What searches are built from: the stats the trade site can filter on, its
// item categories and its leagues. They only change with the game's patches
// and leagues, so they're kept in files.
class TradeDataStore : public QObject
{
    Q_OBJECT

public:
    // The files go in the folder. The client has to outlive the store.
    TradeDataStore(TradeClient *client, const QString &folder, QObject *parent = nullptr);
    ~TradeDataStore() override;

    // What there is so far. Searches can't be built while the stats are
    // empty.
    const TradeData &data() const { return m_data; }
    // The site's league ids, for resolveLeague().
    const QStringList &leagues() const { return m_leagues; }

    // Reads the files the first time, then asks the site for what's missing.
    void refresh();
    bool isRefreshing() const;

Q_SIGNALS:
    // When refresh() is done, whether or not the site answered. Always from
    // the event loop.
    void refreshed();

private:
    // One of the site's data endpoints.
    struct Source
    {
        QString name;               // "stats", "filters" or "leagues"
        QDateTime fetched;          // when the site gave what there is, invalid while there's nothing
        QPointer<TradeReply> reply; // while it's asked for
    };

    QString path(const Source &source) const;
    void take(const Source &source, const QJsonObject &answer);
    void answered(Source &source);

    TradeClient *m_client;
    QString m_folder;
    std::array<Source, 3> m_sources;
    bool m_read = false; // whether the files have been
    TradeData m_data;
    QStringList m_leagues;
};

} // namespace pou
