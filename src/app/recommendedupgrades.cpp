// SPDX-License-Identifier: MIT
#include "app/recommendedupgrades.h"

#include "app/activecharacter.h"
#include "app/settingsstore.h"
#include "app/traderesults.h"
#include "core/evaluation.h"
#include "core/hands.h"
#include "core/slots.h"
#include "services/tradesearcher.h"

#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {

RecommendedUpgrades::RecommendedUpgrades(ActiveCharacter *active, SettingsStore *settings,
                                         TradeSearcher *searcher, TradeDataStore *data, RatesStore *rates,
                                         TradeResults *results, QObject *parent)
    : QAbstractListModel(parent)
    , m_active(active)
    , m_settings(settings)
    , m_results(results)
    , m_finder(searcher, data, rates)
    , m_priorities(settings->settings().pob.priorities)
{
    m_finder.setSettings(settings->settings());
    m_finder.setCharacter(active->character());
    connect(active, &ActiveCharacter::changed, this, &RecommendedUpgrades::refresh);
    connect(active, &ActiveCharacter::pobStatsChanged, this,
            [this] { m_finder.setBuild(m_active->build(), m_active->pobStats()); });
    connect(settings, &SettingsStore::changed, this, [this] {
        m_finder.setSettings(m_settings->settings());
        // which rate resistances higher when they come first
        if (m_settings->settings().pob.priorities != m_priorities)
            refresh();
    });
    connect(&m_finder, &TradeFinder::finished, this, &RecommendedUpgrades::searched);
    connect(&m_finder, &TradeFinder::waiting, this, [this](std::chrono::milliseconds wait) {
        if (!m_searching.isEmpty())
            setReport(m_searching, searchingReport(wait));
    });
    take(recommendedUpgrades(active->character(), m_priorities));
}

int RecommendedUpgrades::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant RecommendedUpgrades::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Recommendation &row = m_rows.at(index.row());
    const auto worn = m_active->character().gear.constFind(row.slot);
    const bool isWorn = worn != m_active->character().gear.cend();
    switch (role) {
    case Qt::DisplayRole:
    case SlotRole:
        return row.slot;
    case NameRole:
        return isWorn ? worn->name : QString();
    case BaseRole:
        return isWorn && worn->baseType != worn->name ? worn->baseType : QString();
    case ScoreRole:
        return row.score ? QString::number(*row.score) : QString();
    case GradeRole:
        return row.grade;
    case LookForRole:
        return wantedText(row.wants);
    case TradeRole:
        return QVariant::fromValue(m_states.value(row.slot).report);
    }
    return {};
}

QHash<int, QByteArray> RecommendedUpgrades::roleNames() const
{
    return {
        {SlotRole, "slot"},   {NameRole, "name"},       {BaseRole, "base"},   {ScoreRole, "score"},
        {GradeRole, "grade"}, {LookForRole, "lookFor"}, {TradeRole, "trade"},
    };
}

void RecommendedUpgrades::setTradeAvailable(bool available)
{
    if (available == m_tradeAvailable)
        return;
    m_tradeAvailable = available;
    Q_EMIT tradeAvailableChanged();
}

void RecommendedUpgrades::choose(const QString &slot)
{
    if (slot == m_chosen || (!slot.isEmpty() && !allSlots().contains(slot)))
        return;
    m_chosen = slot;
    Q_EMIT chosenChanged();
}

QString RecommendedUpgrades::chosenLookFor() const
{
    if (m_chosen.isEmpty())
        return {};
    const Character &character = m_active->character();
    const GearScoreOptions options = gearScoreOptions(character, m_chosen, m_priorities);
    const auto worn = character.gear.constFind(m_chosen);
    if (worn == character.gear.cend()) {
        // a two-handed weapon leaves the offhand empty
        if (offhandOutOfUse(character.gear, character.wielding, m_chosen))
            return {};
        return wantedText(wantedStats(m_chosen, nullptr, options));
    }
    // an item with no gear score, like a unique, has nothing to look for
    const std::optional<GearScore> score = gearScore(*worn, options);
    if (!score || !score->isRated())
        return {};
    return wantedText(wantedStats(m_chosen, &*worn, options));
}

QStringList RecommendedUpgrades::chosenCanRoll() const
{
    if (m_chosen.isEmpty())
        return {};
    const Character &character = m_active->character();
    const auto worn = character.gear.constFind(m_chosen);
    const bool isWorn = worn != character.gear.cend();
    const GearScoreOptions options = gearScoreOptions(character, m_chosen, m_priorities);
    QStringList lines;
    for (const RollRange &r : canRoll(m_chosen, isWorn ? &*worn : nullptr, options))
        lines << rollRangeText(r);
    return lines;
}

bool RecommendedUpgrades::chosenWorn() const
{
    return m_active->character().gear.contains(m_chosen);
}

void RecommendedUpgrades::findChosen()
{
    search(m_chosen);
}

void RecommendedUpgrades::showChosenResults()
{
    showResultsOf(m_chosen);
}

void RecommendedUpgrades::find(int row)
{
    if (row >= 0 && row < m_rows.size())
        search(m_rows.at(row).slot);
}

void RecommendedUpgrades::search(const QString &slot)
{
    const auto worn = m_active->character().gear.constFind(slot);
    if (worn == m_active->character().gear.cend())
        return;
    // which calls off the one before, so it says nothing more
    if (!m_searching.isEmpty())
        setReport(m_searching, {});
    m_searching = slot;
    m_states[slot].found.reset();
    m_states[slot].worn = worn->name;
    setReport(slot, searchingReport());
    m_finder.find(*worn, TradeFinder::Kind::Upgrades);
}

void RecommendedUpgrades::showResults(int row)
{
    if (row >= 0 && row < m_rows.size())
        showResultsOf(m_rows.at(row).slot);
}

void RecommendedUpgrades::showResultsOf(const QString &slot)
{
    const std::optional<TradeFinder::Found> &found = m_states.value(slot).found;
    if (!found || found->total == 0)
        return;
    // another search may have been shown since
    if (!m_results->hasSearched() || m_results->url() != found->url)
        m_results->setSearch(*found);
    Q_EMIT resultsAsked();
}

void RecommendedUpgrades::refresh()
{
    m_priorities = m_settings->settings().pob.priorities;
    m_finder.setCharacter(m_active->character());
    dropStale();

    // Path of Building loading the build changes the character without
    // changing what to say, and that shouldn't call off a search
    const QList<Recommendation> rows = recommendedUpgrades(m_active->character(), m_priorities);
    const auto same = [this, &rows] {
        if (rows.size() != m_rows.size())
            return false;
        for (qsizetype i = 0; i < rows.size(); ++i) {
            if (rows.at(i).slot != m_rows.at(i).slot || rows.at(i).score != m_rows.at(i).score ||
                rows.at(i).wants != m_rows.at(i).wants)
                return false;
            if (m_active->character().gear.value(rows.at(i).slot).name != m_worn.value(rows.at(i).slot))
                return false;
        }
        return true;
    };
    if (!same()) {
        beginResetModel();
        m_finder.cancel();
        m_searching.clear();
        m_states.clear();
        take(rows);
        endResetModel();
    }
    // what to look for in the chosen slot may have changed with the gear
    if (!m_chosen.isEmpty())
        Q_EMIT chosenChanged();
}

void RecommendedUpgrades::dropStale()
{
    // a search is for the item that was worn then
    const QStringList slots = m_states.keys();
    for (const QString &slot : slots) {
        if (m_active->character().gear.value(slot).name == m_states.value(slot).worn)
            continue;
        if (slot == m_searching) {
            m_finder.cancel();
            m_searching.clear();
        }
        m_states.remove(slot);
        if (const int row = rowOf(slot); row >= 0)
            Q_EMIT dataChanged(index(row), index(row), {TradeRole});
    }
}

void RecommendedUpgrades::take(const QList<Recommendation> &rows)
{
    m_rows = rows;
    m_worn.clear();
    for (const Recommendation &r : std::as_const(m_rows))
        m_worn.insert(r.slot, m_active->character().gear.value(r.slot).name);
}

void RecommendedUpgrades::searched()
{
    if (m_searching.isEmpty())
        return;
    State &state = m_states[m_searching];
    state.found = m_finder.found();
    const QString slot = std::exchange(m_searching, QString());
    setReport(slot, state.found ? reportOf(*state.found)
                                : failedReport(m_finder.errorString(), m_finder.errorUrl()));
}

void RecommendedUpgrades::setReport(const QString &slot, const TradeReport &report)
{
    State &state = m_states[slot];
    if (state.report == report)
        return;
    state.report = report;
    if (const int row = rowOf(slot); row >= 0)
        Q_EMIT dataChanged(index(row), index(row), {TradeRole});
    if (slot == m_chosen)
        Q_EMIT chosenChanged();
}

int RecommendedUpgrades::rowOf(const QString &slot) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).slot == slot)
            return i;
    }
    return -1;
}

} // namespace pou
