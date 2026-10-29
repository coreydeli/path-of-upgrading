// SPDX-License-Identifier: MIT
#include "app/traderesults.h"

#include "core/tradesite.h"
#include "services/ratesstore.h"
#include "services/tradesearcher.h"

#include <QDateTime>
#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {

TradeResults::TradeResults(TradeSearcher *searcher, RatesStore *rates, QObject *parent)
    : QAbstractListModel(parent)
    , m_searcher(searcher)
    , m_rates(rates)
{
    // so the prices are worked out again when they come
    connect(rates, &RatesStore::refreshed, this, [this] {
        if (!m_listings.isEmpty())
            Q_EMIT dataChanged(index(0), index(int(m_listings.size()) - 1), {ConvertedRole});
    });
}

TradeResults::~TradeResults()
{
    // calls off what's still asked for
    delete m_fetch;
}

int TradeResults::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_listings.size());
}

QVariant TradeResults::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Listing &listing = m_listings.at(index.row());
    const Item &item = listing.item;
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return item.name;
    case PriceRole:
        return formatPrice(listing.price);
    case ConvertedRole: {
        // the rates are for the league they were last asked for
        if (!listing.price || m_rates->league() != m_league)
            return QString();
        const std::optional<double> exalts = priceInExalts(*listing.price, m_rates->rates());
        return exalts ? formatConvertedPrice(*listing.price, *exalts, m_rates->divineRate()) : QString();
    }
    case AccountRole:
        return listing.account;
    case OnlineRole:
        return listing.online;
    case ListedRole:
        return listedAgo(listing.indexed, QDateTime::currentDateTimeUtc());
    case BaseRole:
        return item.name == item.baseType ? QString() : item.baseType;
    case ModsRole: {
        QList<ResultMod> mods;
        for (const ListingMod &mod : listing.mods)
            mods << ResultMod{mod.text, mod.tag};
        return QVariant::fromValue(mods);
    }
    }
    return {};
}

QHash<int, QByteArray> TradeResults::roleNames() const
{
    return {
        {PriceRole, "price"},   {ConvertedRole, "converted"}, {AccountRole, "account"},
        {OnlineRole, "online"}, {ListedRole, "listed"},       {NameRole, "name"},
        {BaseRole, "base"},     {ModsRole, "mods"},
    };
}

void TradeResults::setSearch(const TradeFinder::Found &found)
{
    const bool wasBusy = isBusy();
    const int wasWaiting = std::exchange(m_waiting, 0);
    const bool hadError = !m_error.isEmpty();
    // what's asked for is about the last one
    delete m_fetch;

    beginResetModel();
    m_listings.clear();
    m_searched = true;
    m_title = found.title;
    m_league = found.league;
    m_total = found.total;
    m_url = found.url;
    m_searchId = found.searchId;
    m_listingIds = found.listingIds;
    m_next = 0;
    m_error.clear();
    endResetModel();

    Q_EMIT searchChanged();
    Q_EMIT moreChanged();
    if (wasBusy)
        Q_EMIT busyChanged();
    if (wasWaiting != 0)
        Q_EMIT waitingChanged();
    if (hadError)
        Q_EMIT errorChanged();
}

void TradeResults::ask()
{
    if (m_next == 0)
        loadMore();
}

void TradeResults::loadMore()
{
    if (isBusy() || !hasMore())
        return;
    // trade searches might have been turned off since the search
    if (const QString why = m_searcher->unavailable(); !why.isEmpty())
        return setError(why);
    setError({});

    m_fetch = m_searcher->fetch(m_listingIds.mid(m_next, tradeFetchLimit), m_searchId);
    connect(m_fetch, &TradeFetch::finished, this, &TradeResults::answered);
    connect(m_fetch, &TradeFetch::waiting, this, [this](std::chrono::milliseconds wait) {
        setWaiting(int(std::chrono::ceil<std::chrono::seconds>(wait).count()));
    });
    Q_EMIT busyChanged();
}

void TradeResults::answered()
{
    const std::unique_ptr<TradeFetch> fetch(m_fetch.data());
    m_fetch.clear();
    setWaiting(0);

    if (fetch->error() != TradeReply::Error::None) {
        setError(fetch->errorString());
    } else {
        // without the item class, which only decides the slot, and the prices
        // are worked out when they're shown
        const QList<Listing> page = listingsFromJson(fetch->answer(), {}, {});
        if (!page.isEmpty()) {
            const int first = int(m_listings.size());
            beginInsertRows({}, first, first + int(page.size()) - 1);
            m_listings += page;
            endInsertRows();
        }
        m_next = std::min<qsizetype>(m_next + tradeFetchLimit, m_listingIds.size());
        Q_EMIT moreChanged();
    }
    Q_EMIT busyChanged();
}

void TradeResults::setError(const QString &why)
{
    if (why == m_error)
        return;
    m_error = why;
    Q_EMIT errorChanged();
}

void TradeResults::setWaiting(int seconds)
{
    if (seconds == m_waiting)
        return;
    m_waiting = seconds;
    Q_EMIT waitingChanged();
}

} // namespace pou
