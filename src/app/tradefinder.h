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
#include <QtQmlIntegration/qqmlintegration.h>
#include <chrono>
#include <optional>

namespace pou {

class RatesStore;
class TradeDataStore;
class TradeSearch;
class TradeSearcher;

// Trade searches for an item you checked, of the kinds the popup's buttons
// ask for. One that finds nothing is tried again looser, and one the site
// says is too complex, smaller.
class TradeFinder : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one trade finder.")

public:
    enum class Kind {
        Upgrades,   // better for your build, by its gear score
        Similar,    // the same stats or better
        Weighted,   // better by the stat weights you set
        CraftBases, // rare ones with your build's key stats, to craft on
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

    // They have to outlive it. The rates turn your budget into exalts.
    TradeFinder(TradeSearcher *searcher, TradeDataStore *data, RatesStore *rates, QObject *parent = nullptr);
    ~TradeFinder() override;

    void setSettings(const Settings &settings);
    // The character you play, whose build profile and weights say what's
    // better and whose ignored stats are left out.
    void setCharacter(const Character &character);
    // Signed out, the trade site takes only the smallest upgrade searches, so
    // they start there. Signed in, they start at their full size again.
    void setSignedIn(bool signedIn);

    // Calls off the one it's busy with. While the trade site's stats are
    // still to come, it waits for them, but it goes by the rates it has.
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
    // While a search waits its turn, as the trade site takes only a few at a
    // time.
    void waiting(std::chrono::milliseconds wait);

private:
    // A search for the item, as loose as it's got.
    struct Plan
    {
        QJsonObject body; // empty when there's nothing to search for, see error
        QString error;
        QString title;
        QString note;
        bool looser = false; // whether there's a looser one for when it finds nothing
        // An upgrade search, which is made smaller when the site says it's too
        // complex.
        bool shrinks = false;
    };

    Plan plan() const;
    Plan upgradePlan() const;
    Plan similarPlan() const;
    Plan weightedPlan() const;
    Plan craftPlan() const;
    Budget budget() const;
    void start();
    void send();
    void answered();
    void fail(const QString &why);
    void failSoon(const QString &why);

    TradeSearcher *m_searcher;
    TradeDataStore *m_data;
    RatesStore *m_rates;
    Settings m_settings;
    Character m_character;
    Item m_item;
    Kind m_kind = Kind::Upgrades;
    int m_level = 0; // how much looser than at first
    bool m_signedIn = false;
    // How small the site needs an upgrade search to be. Later ones start
    // there, rather than spend searches on ones it turns down.
    int m_size;
    Plan m_plan;     // sent last
    int m_finds = 0; // asked for or called off, so far
    bool m_busy = false;
    bool m_waiting = false; // for the trade site's stats
    QPointer<TradeSearch> m_search;
    std::optional<Found> m_found;
    QString m_errorString;
};

} // namespace pou
