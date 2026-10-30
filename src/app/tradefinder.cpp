// SPDX-License-Identifier: MIT
#include "app/tradefinder.h"

#include "core/budget.h"
#include "core/evaluation.h"
#include "core/tradequery.h"
#include "services/ratesstore.h"
#include "services/tradedatastore.h"
#include "services/tradesearcher.h"

#include <QJsonArray>
#include <QLocale>
#include <array>
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

// How a listing's gear score has to compare with yours, and how much of your
// base defences it needs, from clearly better to close, as nothing better may
// be listed.
struct GearLevel
{
    double share;
    double baseShare;
};
constexpr std::array gearLevels = {GearLevel{1.06, 0.9}, GearLevel{1, 0.85}, GearLevel{0.9, 0.75}};

// Smaller and smaller gear searches, for when the trade site says one is too
// complex. Signed out, it takes only a handful of filters.
struct GearSize
{
    int maxFilters;
    bool explicitOnly;
    bool keyStats; // asked for apart from the weighted sum
    bool countOnly = false;
};
constexpr std::array gearSizes = {GearSize{40, false, true}, GearSize{20, true, true},
                                  GearSize{10, true, false}, GearSize{6, true, false},
                                  GearSize{6, true, false, true}};

// What a gear search looks at.
QString scoredBy(const GearSearch &search)
{
    constexpr qsizetype shown = 5;
    QStringList parts = search.parts.mid(0, shown);
    if (search.parts.size() > shown)
        parts << u"%1 more"_s.arg(search.parts.size() - shown);
    QString note = parts.isEmpty() ? u"Scored by your build's weights."_s
                                   : u"Scored by your build's weights for %1."_s.arg(
                                         QLocale(QLocale::English).createSeparatedList(parts));
    if (search.needKeyStats > 0)
        note += u" Each has at least %1 of these at a decent roll: %2."_s.arg(
            QString::number(search.needKeyStats), search.keyStats.join(u", "));
    return note;
}

} // namespace

TradeFinder::TradeFinder(TradeSearcher *searcher, TradeDataStore *data, RatesStore *rates, QObject *parent)
    : QObject(parent)
    , m_searcher(searcher)
    , m_data(data)
    , m_rates(rates)
{
    // queued, so the app has taken the site's leagues from the data by then
    connect(
        m_data, &TradeDataStore::refreshed, this,
        [this] {
            if (m_waiting) {
                m_waiting = false;
                start();
            }
        },
        Qt::QueuedConnection);
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
    // while trade searches are off, the site isn't asked for its stats either
    if (m_data->data().stats.isEmpty() && m_searcher->unavailable().isEmpty()) {
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
    // trade searches can be turned off while it waits for the stats
    if (const QString why = m_searcher->unavailable(); !why.isEmpty())
        return failSoon(why);
    if (m_data->data().stats.isEmpty()) {
        const QString why = m_data->errorString();
        return failSoon(why.isEmpty() ? u"Searches need the trade site's list of stats."_s : why);
    }
    // which come from their file the first time, and from poe.ninja again
    // once they're an hour old
    m_rates->refresh(m_searcher->league().tradeId());
    send();
}

TradeFinder::Plan TradeFinder::plan() const
{
    switch (m_kind) {
    case Kind::Upgrades:
        return upgradePlan();
    case Kind::Weighted:
        return weightedPlan();
    case Kind::CraftBases:
        return craftPlan();
    case Kind::Similar:
        break;
    }
    return similarPlan();
}

TradeFinder::Plan TradeFinder::upgradePlan() const
{
    const PobPriorities &priorities = m_settings.pob.priorities;
    const Evaluation evaluation = evaluate(m_item, m_character, priorities, {});
    // There's no gear score without a build profile, or for flasks and
    // charms, and a unique's stats are what they are.
    using Unrated = GearScore::Unrated;
    const std::optional<GearScore> &score = evaluation.gear;
    if (!score || score->unrated == Unrated::FlaskOrCharm || score->unrated == Unrated::Unique)
        return similarPlan();
    // of your rings, the one it would take the place of
    const QString &slot = evaluation.comparison.targetSlot;
    const QList<SearchWeight> weights =
        searchWeights(m_item.slot, gearScoreOptions(m_character, slot, priorities));
    if (weights.isEmpty())
        return {.error = u"Your build profile wants none of the stats this slot can have."_s};

    const GearLevel &level = gearLevels.at(m_level);
    const GearSize &size = gearSizes.at(m_size);
    GearSearchOptions options;
    options.status = m_settings.trade.status;
    options.ignoreRules = m_character.ignored;
    options.share = level.share;
    options.baseShare = level.baseShare;
    options.maxFilters = size.maxFilters;
    options.explicitOnly = size.explicitOnly;
    if (!size.keyStats)
        options.keyStatShare = 0;
    options.countOnly = size.countOnly;
    const GearSearch search = gearSearch(m_item, weights, m_data->data(), options);
    if (search.filters == 0)
        return {.error = u"None of your build's stats for this slot could be matched on the trade site."_s};

    Plan plan{
        .body = search.body,
        .title = score->isRated() ? u"Better for your build than %1 (gear score %2)"_s.arg(
                                        m_item.name, QString::number(score->score))
                                  : u"Better for your build than %1"_s.arg(m_item.name),
        // the simplest one doesn't ask for your gear score at all
        .looser = !search.countOnly && m_level + 1 < int(gearLevels.size()),
        .shrinks = true,
    };
    if (search.countOnly)
        plan.note =
            u"The trade site only takes a simple search, so each has at least %1 of these at a decent "
            u"roll: %2. Cheapest first."_s.arg(QString::number(search.needKeyStats),
                                               search.keyStats.join(u", "));
    else if (m_level == 0 && m_size > 0)
        plan.note =
            u"Scored by your build's main weights, as the trade site limits how many filters a search "
            u"can have. Best for your build first."_s;
    else if (m_level == 0)
        plan.note = scoredBy(search) + u" Best for your build first."_s;
    else if (level.share >= 1)
        plan.note = u"No clear upgrade is listed right now, so these are a little better than yours."_s;
    else
        plan.note = u"Nothing better is listed right now, so these come within %1% of yours."_s.arg(
            qRound((1 - level.share) * 100));
    return plan;
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

TradeFinder::Plan TradeFinder::weightedPlan() const
{
    WeightedSearchOptions options;
    options.status = m_settings.trade.status;
    options.ignoreRules = m_character.ignored;
    options.preferLocal = prefersLocal(m_item);
    const WeightedSearch search = weightedSearch(m_item, m_character.weights, m_data->data(), options);
    // with nothing to beat, almost anything would
    if (search.current <= 0 && search.unmatched.isEmpty())
        return {.error = u"None of your stat weights are on this item, so almost anything would beat it. Set "
                         u"weights for its stats, or find upgrades for your build."_s};
    if (search.current <= 0)
        return {.error = u"None of your weighted stats on this item could be matched on the trade site."_s};

    Plan plan{.body = search.body, .title = u"Beats your %1 by your weights"_s.arg(m_item.name)};
    if (!search.unmatched.isEmpty()) {
        QStringList quoted;
        for (const QString &key : search.unmatched)
            quoted << u'"' + key + u'"';
        plan.note = u"The trade site has no filter for %1, so %2 counted."_s.arg(
            QLocale(QLocale::English).createSeparatedList(quoted),
            search.unmatched.size() == 1 ? u"it isn't"_s : u"they aren't"_s);
    }
    return plan;
}

TradeFinder::Plan TradeFinder::craftPlan() const
{
    // which leave resistances out, so what you need of them doesn't matter
    const QList<KeyStat> keyStats = topStats(m_item.slot, 2, gearScoreOptions(m_character, m_item.slot));
    if (keyStats.isEmpty())
        return {.error = u"Your build profile has no key stats for this slot to look for."_s};
    SearchOptions options;
    options.status = m_settings.trade.status;
    options.ignoreRules = m_character.ignored;
    options.preferLocal = prefersLocal(m_item);
    const CraftBaseSearch search = craftBaseSearch(m_item, keyStats, m_data->data(), options);
    QStringList wanted;
    for (const KeyStat &stat : keyStats) {
        if (!search.unmatched.contains(stat.key))
            wanted << stat.label;
    }
    if (wanted.isEmpty())
        return {.error =
                    u"None of your build's key stats for this slot could be matched on the trade site."_s};

    const QString stats = QLocale(QLocale::English).createSeparatedList(wanted);
    Plan plan{
        .body = search.body,
        .title = u"Craftable %1 with %2"_s.arg(m_item.itemClass, stats),
    };
    plan.note =
        search.emptyAffixFilter
            ? u"Each is rare and not corrupted, with %1 at a decent roll and room for another prefix or "
              u"suffix. Cheapest first."_s.arg(stats)
            : u"Each is rare and not corrupted, with %1 at a decent roll. The trade site can't be asked "
              u"for room for another prefix or suffix, so look before you buy. Cheapest first."_s.arg(stats);
    return plan;
}

// Yours, at poe.ninja's rate for the league when you'd rather and it has
// one, or else the one you typed in.
Budget TradeFinder::budget() const
{
    Budget budget = m_settings.trade.budget;
    if (m_settings.trade.autoRate && m_rates->divineRate() > 0)
        budget.divineRate = m_rates->divineRate();
    return budget;
}

void TradeFinder::send()
{
    m_plan = plan();
    if (m_plan.body.isEmpty())
        return failSoon(m_plan.error);
    applyBudget(m_plan.body, budget());
    m_search = m_searcher->search(m_plan.body);
    connect(m_search, &TradeSearch::finished, this, &TradeFinder::answered);
    connect(m_search, &TradeSearch::waiting, this, &TradeFinder::waiting);
}

void TradeFinder::answered()
{
    const std::unique_ptr<TradeSearch> search(m_search.data());
    m_search.clear();
    if (search->error() == TradeSearch::Error::TooComplex && m_plan.shrinks) {
        if (m_size + 1 == int(gearSizes.size()))
            return fail(u"The trade site says this search has too many filters even at its smallest."_s);
        ++m_size;
        return send();
    }
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
