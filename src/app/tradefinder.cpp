// SPDX-License-Identifier: MIT
#include "app/tradefinder.h"

#include "core/tradequery.h"
#include "services/tradedatastore.h"
#include "services/tradesearcher.h"

#include <QJsonArray>
#include <memory>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Armour and weapons have local versions of their defence and damage mods.
bool prefersLocal(const Item &item)
{
    static const QStringList local = {u"Weapon"_s,      u"Offhand"_s, u"Helmet"_s,
                                      u"Body Armour"_s, u"Gloves"_s,  u"Boots"_s};
    return local.contains(item.slot);
}

// Whether a search asks for more than the kind of item, which would be
// every listing of it.
bool asksForAnything(const QJsonObject &body)
{
    const QJsonObject query = body.value(u"query").toObject();
    return !query.value(u"stats").toArray().isEmpty() || query.contains(u"name") ||
           query.value(u"filters").toObject().contains(u"equipment_filters");
}

} // namespace

TradeFinder::TradeFinder(TradeSearcher *searcher, TradeDataStore *data, QObject *parent)
    : QObject(parent)
    , m_searcher(searcher)
    , m_data(data)
{
    connect(m_data, &TradeDataStore::refreshed, this, [this] {
        if (m_waiting) {
            m_waiting = false;
            start();
        }
    });
}

TradeFinder::~TradeFinder()
{
    // calls off what's still asked for
    delete m_search;
}

void TradeFinder::setSettings(const Settings &settings)
{
    m_settings = settings;
}

void TradeFinder::setCharacter(const Character &character)
{
    m_character = character;
}

void TradeFinder::find(const Item &item, Kind kind)
{
    cancel();
    m_item = item;
    m_kind = kind;
    m_level = 0;
    m_found.reset();
    m_errorString.clear();
    m_busy = true;
    // and the site isn't asked for its stats either
    if (const QString why = m_searcher->unavailable(); !why.isEmpty())
        return failSoon(why);
    if (m_data->data().stats.isEmpty()) {
        m_waiting = true;
        m_data->refresh();
        return;
    }
    start();
}

void TradeFinder::cancel()
{
    ++m_finds;
    delete m_search;
    m_waiting = false;
    m_busy = false;
}

void TradeFinder::start()
{
    if (m_data->data().stats.isEmpty()) {
        const QString why = m_data->errorString();
        return failSoon(why.isEmpty() ? u"Searches need the trade site's list of stats."_s : why);
    }
    send();
}

TradeFinder::Plan TradeFinder::plan() const
{
    switch (m_kind) {
    case Kind::Similar:
        break;
    }
    return similarPlan();
}

TradeFinder::Plan TradeFinder::similarPlan() const
{
    SimilarSearchOptions options;
    options.status = m_settings.trade.status;
    options.ignoreRules = m_character.ignored;
    options.preferLocal = prefersLocal(m_item);
    options.level = m_level;
    const SimilarSearch search = similarSearch(m_item, m_data->data(), options);
    if (!asksForAnything(search.body))
        return {.error = u"None of this item's stats could be matched on the trade site."_s};

    Plan plan{
        .body = search.body,
        .title = u"Same stats or better than %1"_s.arg(m_item.name),
        .looser = search.level + 1 < similarSearchLevels(),
    };
    if (search.unique && search.need == 0)
        plan.note = u"Nothing matched its stats, so these are all the %1 listed."_s.arg(m_item.name);
    else if (search.level > 0)
        plan.note = u"Nothing matched your exact values, so these have %1, on %2 of the %3 stats."_s.arg(
            search.levelLabel, QString::number(search.need), QString::number(search.total));
    return plan;
}

void TradeFinder::send()
{
    m_plan = plan();
    if (m_plan.body.isEmpty())
        return failSoon(m_plan.error);
    m_search = m_searcher->search(m_plan.body);
    connect(m_search, &TradeSearch::finished, this, &TradeFinder::answered);
}

void TradeFinder::answered()
{
    const std::unique_ptr<TradeSearch> search(m_search.data());
    m_search.clear();
    if (search->error() != TradeSearch::Error::None)
        return fail(search->errorString());
    if (search->total() == 0 && m_plan.looser) {
        ++m_level;
        return send();
    }
    m_found = Found{
        .kind = m_kind,
        .item = m_item,
        .title = m_plan.title,
        .note = m_plan.note,
        .league = search->league(),
        .url = search->url(),
        .searchId = search->id(),
        .total = search->total(),
        .listingIds = search->listingIds(),
    };
    m_busy = false;
    Q_EMIT finished();
}

void TradeFinder::fail(const QString &why)
{
    m_errorString = why;
    m_busy = false;
    Q_EMIT finished();
}

// From the event loop, unless it's been called off or asked again since.
void TradeFinder::failSoon(const QString &why)
{
    QMetaObject::invokeMethod(
        this,
        [this, why, find = m_finds] {
            if (find == m_finds)
                fail(why);
        },
        Qt::QueuedConnection);
}

} // namespace pou
