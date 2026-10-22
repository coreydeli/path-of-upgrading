// SPDX-License-Identifier: MIT
#pragma once

#include "core/budget.h"
#include "core/league.h"
#include "core/pobverdict.h"
#include "core/tradequery.h"

#include <QString>

class QSettings;

namespace pou {

// What you choose in the settings window, kept between runs.

struct TradeSettings
{
    bool enabled = true; // search the trade site at all
    League league;       // an empty name follows the current league
    ListingStatus status = ListingStatus::Available;
    Budget budget;        // with the divine rate you typed in
    bool autoRate = true; // or poe.ninja's instead

    friend bool operator==(const TradeSettings &, const TradeSettings &) = default;
};

struct PobSettings
{
    bool enabled = true; // judge items with Path of Building when it's there
    // Path of Building's folder, for a copy that isn't the Flatpak. Empty to
    // look in the usual places.
    QString location;
    double damageWeight = PobVerdictOptions().damageWeight;
    PobPriorities priorities;

    friend bool operator==(const PobSettings &, const PobSettings &) = default;
};

enum class PopupPosition { Cursor, Left, Right, Top };

struct OverlaySettings
{
    QString hotkey = QStringLiteral("F2");
    QString resultsHotkey = QStringLiteral("Ctrl+Shift+D");
    bool advancedCopy = false;  // copy with Ctrl+Alt+C, which tells prefixes from suffixes
    bool watchClipboard = true; // also check gear you copy yourself
    PopupPosition position = PopupPosition::Cursor;
    double scale = 1;         // popup size
    bool compact = true;      // the rest of the popup behind More
    bool colorblind = false;  // blue for better and orange for worse
    int autoHideSeconds = 12; // while the mouse isn't over the popup
    bool paused = false;      // the hotkey and the clipboard do nothing

    friend bool operator==(const OverlaySettings &, const OverlaySettings &) = default;
};

struct Settings
{
    QString character; // id of the one you play, see CharacterStore
    TradeSettings trade;
    PobSettings pob;
    OverlaySettings overlay;

    friend bool operator==(const Settings &, const Settings &) = default;
};

// Anything missing or out of range reads as its default.
Settings loadSettings(const QSettings &store);
void saveSettings(QSettings &store, const Settings &settings);

} // namespace pou
