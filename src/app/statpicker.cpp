// SPDX-License-Identifier: MIT
#include "app/statpicker.h"

#include "app/itemhistory.h"
#include "core/catalog.h"
#include "core/statnorm.h"
#include "core/tradedata.h"
#include "services/tradedatastore.h"

#include <QCollator>
#include <QLocale>
#include <QSet>
#include <algorithm>
#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// "Explicit, Rune. On 3 of the items you've checked"
QString noteFor(QStringList parts, int seen)
{
    parts.removeAll(QString());
    if (seen == 1)
        parts << u"On one of the items you've checked"_s;
    else if (seen > 1)
        parts << u"On %1 of the items you've checked"_s.arg(seen);
    return parts.join(u". "_s);
}

// Where the stat can come from. An item never has a total, so a stat that
// only comes as one is of no use to weight or to ignore, and has none.
QStringList typeLabels(const CatalogEntry &entry)
{
    QStringList labels;
    for (const QString &type : entry.types) {
        const QString label = statTypeLabel(type);
        if (type != u"pseudo" && !labels.contains(label))
            labels << label;
    }
    return labels;
}

} // namespace

StatPicker::StatPicker(TradeDataStore *data, ItemHistory *history, QObject *parent)
    : QAbstractListModel(parent)
    , m_data(data)
    , m_history(history)
{
    connect(data, &TradeDataStore::refreshed, this, [this] {
        // only while it has no stats, so a list in use doesn't shift under the pointer
        if (m_opened && !m_hadStats)
            rebuild();
    });
}

int StatPicker::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant StatPicker::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Stat &stat = m_stats.at(m_rows.at(index.row()));
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return stat.name;
    case KeyRole:
        return stat.key;
    case NoteRole:
        return stat.note;
    case TakenRole:
        return m_taken.contains(stat.key);
    }
    return {};
}

QHash<int, QByteArray> StatPicker::roleNames() const
{
    return {{KeyRole, "key"}, {NameRole, "name"}, {NoteRole, "note"}, {TakenRole, "taken"}};
}

void StatPicker::setQuery(const QString &query)
{
    if (query == m_query)
        return;
    m_query = query;
    const QStringList words = query.simplified().split(u' ', Qt::SkipEmptyParts);
    if (words != m_words) {
        m_words = words;
        beginResetModel();
        filter();
        endResetModel();
    }
    Q_EMIT queryChanged();
}

void StatPicker::setTaken(const QStringList &keys)
{
    if (keys == m_taken)
        return;
    m_taken = keys;
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_rows.size()) - 1), {TakenRole});
    Q_EMIT takenChanged();
}

void StatPicker::setSiteAllowed(bool allowed)
{
    m_siteAllowed = allowed;
}

void StatPicker::setTradeOffered(bool offered)
{
    m_tradeOffered = offered;
    updateMessage();
}

void StatPicker::refresh()
{
    // from the files, as opening the picker asks the site for nothing
    m_data->load();
    m_opened = true;
    rebuild();
}

void StatPicker::rebuild()
{
    const SeenStats &seen = m_history->seenStats();
    const StatCatalog catalog = buildCatalog(m_data->data().stats, seen);
    m_hadStats = !m_data->data().stats.isEmpty();

    QHash<QString, int> seenKeys; // on items of any class
    for (const QHash<QString, int> &counts : seen) {
        for (auto it = counts.cbegin(); it != counts.cend(); ++it)
            seenKeys[it.key()] += it.value();
    }

    QList<Stat> stats;
    QSet<QString> keys;
    const auto add = [&stats, &keys](Stat stat) {
        if (!keys.contains(stat.key)) {
            keys.insert(stat.key);
            stats << std::move(stat);
        }
    };
    for (const QString &key : equipmentKeys()) {
        const int n = seenKeys.value(key);
        add({.key = key,
             .name = statName(key),
             .note = noteFor({propertyNote(key)}, n),
             .property = true,
             .seen = n});
    }
    for (const CatalogEntry &entry : catalog.entries) {
        const QStringList labels = typeLabels(entry);
        if (labels.isEmpty())
            continue;
        const QString key = itemKeyFromText(entry.text);
        const int n = entry.seenOn();
        add({.key = key,
             .name = key,
             .note = noteFor({labels.join(u", ")}, n),
             .property = false,
             .seen = n});
    }
    // the rest of those on your items, which the site has no filter for but
    // still count in the popup
    const QString notOnSite = catalog.entries.isEmpty() ? QString() : u"Not on the trade site"_s;
    for (auto it = seenKeys.cbegin(); it != seenKeys.cend(); ++it) {
        const QString &key = it.key();
        add({.key = key,
             .name = statName(key),
             .note = noteFor({propertyNote(key), notOnSite}, it.value()),
             .property = isItemProperty(key),
             .seen = it.value()});
    }

    // stat texts are English whatever language the app is in
    const QCollator collator{QLocale(QLocale::English)};
    std::ranges::stable_sort(stats, [&collator](const Stat &a, const Stat &b) {
        if (a.property != b.property)
            return a.property;
        if (a.seen != b.seen)
            return a.seen > b.seen;
        return collator.compare(a.name, b.name) < 0;
    });

    beginResetModel();
    m_stats = std::move(stats);
    filter();
    endResetModel();
    updateMessage();
}

void StatPicker::filter()
{
    m_rows.clear();
    for (qsizetype i = 0; i < m_stats.size(); ++i) {
        const QString &name = m_stats.at(i).name;
        if (std::ranges::all_of(
                m_words, [&name](const QString &word) { return name.contains(word, Qt::CaseInsensitive); }))
            m_rows << i;
    }
}

QString StatPicker::missingStats() const
{
    if (!m_data->data().stats.isEmpty() || !m_tradeOffered)
        return {};
    if (!m_siteAllowed)
        return u"Trade search brings the trade site's list of stats, so for now you only see the item's own "
               u"stats and those on items you've checked."_s;
    if (m_data->isRefreshing())
        return u"Getting the list of stats from the trade site."_s;
    if (m_data->errorString().isEmpty())
        return u"You get the trade site's list of stats with your first trade search, or when you press "
               u"Refresh "
               u"on the Trade page. Until then you only see the item's own stats and those on items you've "
               u"checked."_s;
    return u"Couldn't get the trade site's list of stats, so you only see the item's own stats and those on "
           u"items you've checked."_s;
}

void StatPicker::updateMessage()
{
    if (const QString message = missingStats(); message != m_message) {
        m_message = message;
        Q_EMIT messageChanged();
    }
}

} // namespace pou
