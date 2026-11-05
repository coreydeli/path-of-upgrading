// SPDX-License-Identifier: MIT
#pragma once

#include "app/tradefinder.h"
#include "app/tradereport.h"
#include "core/recommendations.h"
#include "core/settings.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <optional>

namespace pou {

class ActiveCharacter;
class RatesStore;
class SettingsStore;
class TradeDataStore;
class TradeResults;
class TradeSearcher;

// The slots of the character you play to look for upgrades in first, with
// what to look for in each (see recommendedUpgrades()), and a trade search for
// the worn item's upgrades. It has a trade finder of its own, so a search here
// doesn't touch the popup, and what it finds goes to the results window only
// when you ask to see it.
class RecommendedUpgrades : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // Whether trade searches are on, which the Find buttons need.
    Q_PROPERTY(bool tradeAvailable READ isTradeAvailable NOTIFY tradeAvailableChanged)

public:
    enum Role {
        SlotRole = Qt::UserRole + 1, // "Boots"
        NameRole,                    // what's worn there, empty when nothing is
        BaseRole,                    // its base type, empty when that's its name
        ScoreRole,                   // its gear score, "34", empty when nothing is worn
        GradeRole,                   // "D"
        LookForRole,                 // "Life 55 or more, Fire res 24 or more"
        TradeRole,                   // a tradeReport, what its search is up to
    };

    // They have to outlive it.
    RecommendedUpgrades(ActiveCharacter *active, SettingsStore *settings, TradeSearcher *searcher,
                        TradeDataStore *data, RatesStore *rates, TradeResults *results,
                        QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // For the app to hand what it tells its own finder: Path of Building, and
    // whether you're signed in.
    TradeFinder &finder() { return m_finder; }
    void setTradeAvailable(bool available);
    bool isTradeAvailable() const { return m_tradeAvailable; }

    // Searches the trade site for upgrades to what's worn in the row's slot. It
    // does nothing for an empty slot.
    Q_INVOKABLE void find(int row);
    // Shows what the row's search found in the results window. That asks to be
    // shown, see resultsAsked().
    Q_INVOKABLE void showResults(int row);

Q_SIGNALS:
    void tradeAvailableChanged();
    void resultsAsked();

private:
    struct State
    {
        TradeReport report;
        std::optional<TradeFinder::Found> found;
    };

    void refresh();
    void take(const QList<Recommendation> &rows);
    void searched();
    void setReport(const QString &slot, const TradeReport &report);
    int rowOf(const QString &slot) const;

    ActiveCharacter *m_active;
    SettingsStore *m_settings;
    TradeResults *m_results;
    TradeFinder m_finder;
    QList<Recommendation> m_rows;
    QHash<QString, QString> m_worn; // the name of what each was for
    PobPriorities m_priorities;     // what the scores were worked out with
    QHash<QString, State> m_states;
    QString m_searching; // the slot the finder is busy with
    bool m_tradeAvailable = false;
};

} // namespace pou
