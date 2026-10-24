// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"
#include "core/item.h"
#include "core/settings.h"

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <optional>

namespace pou {

class TradeDataStore;
class TradeSearch;
class TradeSearcher;

// Trade searches for an item you checked, of the kinds the popup's buttons
// ask for. One that finds nothing is tried again looser.
class TradeFinder : public QObject
{
    Q_OBJECT

public:
    enum class Kind {
        Upgrades, // better for your build, by its gear score
        Similar,  // the same stats or better
    };
    Q_ENUM(Kind)

    // What a search found, and how.
    struct Found
    {
        Kind kind = Kind::Upgrades;
        Item item;     // it was for
        QString title; // "Same stats or better than Doom Visage"
        QString note;  // how it had to search, when that needs saying
        QString league;
        QUrl url; // the listings on the trade site
        QString searchId;
        int total = 0; // which can be none
        QStringList listingIds;
    };

    // They have to outlive it.
    TradeFinder(TradeSearcher *searcher, TradeDataStore *data, QObject *parent = nullptr);
    ~TradeFinder() override;

    void setSettings(const Settings &settings);
    // The character you play, whose build profile says what's better and
    // whose ignored stats are left out.
    void setCharacter(const Character &character);

    // Calls off the one it's busy with. While the trade site's stats are
    // still to come, it waits for them.
    void find(const Item &item, Kind kind);
    // Without finishing.
    void cancel();
    bool isBusy() const { return m_busy; }

    // Once it has finished, what it found, or why it found nothing.
    const std::optional<Found> &found() const { return m_found; }
    QString errorString() const { return m_errorString; }

Q_SIGNALS:
    // Always from the event loop.
    void finished();

private:
    // A search for the item, as loose as it's got.
    struct Plan
    {
        QJsonObject body; // empty when there's nothing to search for, see error
        QString error;
        QString title;
        QString note;
        bool looser = false; // whether there's a looser one for when it finds nothing
    };

    Plan plan() const;
    Plan upgradePlan() const;
    Plan similarPlan() const;
    void start();
    void send();
    void answered();
    void fail(const QString &why);
    void failSoon(const QString &why);

    TradeSearcher *m_searcher;
    TradeDataStore *m_data;
    Settings m_settings;
    Character m_character;
    Item m_item;
    Kind m_kind = Kind::Upgrades;
    int m_level = 0; // how much looser than at first
    Plan m_plan;     // sent last
    int m_finds = 0; // asked for or called off, so far
    bool m_busy = false;
    bool m_waiting = false; // for the trade site's stats
    QPointer<TradeSearch> m_search;
    std::optional<Found> m_found;
    QString m_errorString;
};

} // namespace pou
