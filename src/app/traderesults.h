// SPDX-License-Identifier: MIT
#pragma once

#include "app/tradefinder.h"
#include "core/listing.h"

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class RatesStore;
class TradeFetch;
class TradeSearcher;

// One mod line of a listed item.
struct ResultMod
{
    Q_GADGET
    QML_VALUE_TYPE(resultMod)

    Q_PROPERTY(QString text MEMBER text)
    // enchant, rune, implicit, fractured, explicit, desecrated or crafted
    Q_PROPERTY(QString tag MEMBER tag)

public:
    QString text;
    QString tag;

    friend bool operator==(const ResultMod &, const ResultMod &) = default;
};

// The listings of the last trade search the popup ran, as the results window
// shows them. Each search replaces the last one's, even when it found none,
// and one that failed or was called off leaves them. Nothing is asked of the trade site
// until ask() says you want to see them, and then only the first page. The
// next ones come with loadMore().
class TradeResults : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one list of trade results.")

    // Whether there's been a search to show, found listings or not.
    Q_PROPERTY(bool searched READ hasSearched NOTIFY searchChanged)
    Q_PROPERTY(QString title READ title NOTIFY searchChanged)
    Q_PROPERTY(QString league READ league NOTIFY searchChanged)
    // How many the search found, which can be more than there are to load.
    Q_PROPERTY(int total READ total NOTIFY searchChanged)
    // How many of them the site lists, which is the first hundred at most.
    Q_PROPERTY(int available READ available NOTIFY searchChanged)
    // The search on the trade site, to open in the browser.
    Q_PROPERTY(QUrl url READ url NOTIFY searchChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    // How many seconds until the trade site takes the request, or 0.
    Q_PROPERTY(int waiting READ waiting NOTIFY waitingChanged)
    // Why the last request got nothing, empty when it did.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    // Whether loadMore() has listings left to ask for.
    Q_PROPERTY(bool more READ hasMore NOTIFY moreChanged)

public:
    enum Role {
        PriceRole = Qt::UserRole + 1, // as the seller put it, "3 div"
        ConvertedRole,                // "about 2100 ex", empty without a rate or when it says no more
        AccountRole,                  // "name#1234"
        OnlineRole,
        ListedRole, // "5 minutes ago"
        NameRole,   // its base type for one without a name
        BaseRole,   // empty when that's its name
        ModsRole,   // a list of resultMod
    };

    // They have to outlive it. The rates turn prices into exalts or divines.
    TradeResults(TradeSearcher *searcher, RatesStore *rates, QObject *parent = nullptr);
    ~TradeResults() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Shows what a search found instead of the last, and calls off the
    // request for those. It asks for nothing yet.
    void setSearch(const TradeFinder::Found &found);

    // Asks for the first page of the listings, unless it has them or there
    // are none. It does nothing while trade searches are off.
    Q_INVOKABLE void ask();
    // Asks for the next page, ten more.
    Q_INVOKABLE void loadMore();

    bool hasSearched() const { return m_searched; }
    QString title() const { return m_title; }
    QString league() const { return m_league; }
    int total() const { return m_total; }
    int available() const { return int(m_listingIds.size()); }
    QUrl url() const { return m_url; }
    bool isBusy() const { return !m_fetch.isNull(); }
    int waiting() const { return m_waiting; }
    QString error() const { return m_error; }
    bool hasMore() const { return m_next < m_listingIds.size(); }

Q_SIGNALS:
    void searchChanged();
    void busyChanged();
    void waitingChanged();
    void errorChanged();
    void moreChanged();

private:
    void answered();
    void setError(const QString &why);
    void setWaiting(int seconds);

    TradeSearcher *m_searcher;
    RatesStore *m_rates;
    bool m_searched = false;
    QString m_title;
    QString m_league;
    int m_total = 0;
    QUrl m_url;
    QString m_searchId;
    QStringList m_listingIds;
    qsizetype m_next = 0; // the first of them not asked for
    QList<Listing> m_listings;
    QPointer<TradeFetch> m_fetch;
    int m_waiting = 0;
    QString m_error;
};

} // namespace pou
