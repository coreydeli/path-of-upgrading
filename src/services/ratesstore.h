// SPDX-License-Identifier: MIT
#pragma once

#include "core/rates.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace pou {

// What currencies are worth in a league, from poe.ninja, for pricing
// listings in Exalted Orbs and turning a budget in Divine Orbs into them.
// Prices move through the day, so they're asked for again once they're an
// hour old. Each league's are kept in a file, and the older ones do while
// poe.ninja can't be reached.
class RatesStore : public QObject
{
    Q_OBJECT

public:
    // The files go in the folder. The network has to outlive the store.
    RatesStore(QNetworkAccessManager *network, const QString &folder, QObject *parent = nullptr);
    ~RatesStore() override;

    // The trade site's id for the league they're for.
    QString league() const { return m_league; }
    // What each currency is worth in Exalted Orbs, by the ids the trade site
    // prices listings in. Empty while there are none.
    const CurrencyRates &rates() const { return m_rates; }
    // How many Exalted Orbs a Divine Orb is worth, to one decimal, or 0 while
    // there are no rates.
    double divineRate() const;
    // When poe.ninja gave them, invalid while there are none.
    QDateTime fetched() const { return m_fetched; }
    // Why the last refresh didn't get new ones, or empty when it did.
    QString errorString() const { return m_errorString; }

    // The rates for the league, by its trade site id (League::tradeId()):
    // from its file the first time, then from poe.ninja once they're an hour
    // old, or right away when forced. It's fine to call before every price
    // check, as after poe.ninja lets it down it waits five minutes before
    // asking again, unless forced. There are none for a league with no
    // trading, which has an empty id.
    void refresh(const QString &league, bool force = false);
    bool isRefreshing() const { return !m_reply.isNull(); }

Q_SIGNALS:
    // When refresh() is done, whether or not poe.ninja answered. Always from
    // the event loop.
    void refreshed();

private:
    QString path() const;
    void read();
    bool take(const QJsonObject &overview);
    void answered();
    void fail(const QString &why);

    QNetworkAccessManager *m_network;
    QString m_folder;
    QString m_league;
    CurrencyRates m_rates;
    QDateTime m_fetched;
    QDateTime m_failed; // when poe.ninja last let it down
    QString m_errorString;
    QPointer<QNetworkReply> m_reply; // while they're asked for
};

} // namespace pou
