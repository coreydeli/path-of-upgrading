// SPDX-License-Identifier: MIT
#pragma once

#include "core/budget.h"
#include "core/league.h"
#include "core/placement.h"
#include "core/pobverdict.h"
#include "core/tradequery.h"

#include <QString>

class QSettings;

namespace pou {

// What you choose in the settings window, kept between runs.

// The limits on how often the rate is asked for again.
constexpr int minRateMinutes = 1;
constexpr int maxRateMinutes = 60;

// Whether this is a beta, as the release builds of one say with POU_BETA.
// Trade searches are only in a beta, where they start on until you choose. A
// release leaves them out, whatever the settings say.
bool isBetaBuild();

struct TradeSettings
{
    bool enabled = true; // search the trade site at all
    League league;       // an empty name follows the current league
    ListingStatus status = ListingStatus::Available;
    Budget budget;        // with the divine rate you typed in
    bool autoRate = true; // or poe.ninja's instead
    // How often poe.ninja's rate is asked for again while that's in use.
    int rateMinutes = 10;

    friend bool operator==(const TradeSettings &, const TradeSettings &) = default;
};

// Why there are no trade searches with these settings, or empty when there
// are. When they're off it says how to turn them on.
QString tradeUnavailable(const TradeSettings &settings);

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
    int autoHideSeconds = 60; // while the mouse isn't over the popup
    bool paused = false;      // the hotkey and the clipboard do nothing
    // Starting with the desktop has been settled, by the first start turning it
    // on or by you choosing. Until then it's on by default.
    bool startChosen = false;

    friend bool operator==(const OverlaySettings &, const OverlaySettings &) = default;
};

// Whose colors the app uses.
enum class ColorTheme {
    App,    // its own, from the color scheme that comes with it
    Desktop // the desktop's
};
QString colorThemeId(ColorTheme theme);
ColorTheme colorThemeFromId(QStringView id);

// How much is drawn besides the colors.
enum class Decorations {
    Full, // cut corners and the popup's animation
    Plain
};
QString decorationsId(Decorations decorations);
Decorations decorationsFromId(QStringView id);

// What headingFont says for "the same as the text".
constexpr QStringView sameAsText = u"text";

// The smallest and largest text size to ask for, in points.
constexpr double smallestTextSize = 6;
constexpr double largestTextSize = 32;

struct AppearanceSettings
{
    ColorTheme colors = ColorTheme::App;
    Decorations decorations = Decorations::Full;
    // Empty is the app's own heading font, "text" is the text font, and else
    // a font family.
    QString headingFont;
    QString textFont;    // a font family, empty is the app's own
    double textSize = 0; // in points, 0 is the desktop's

    friend bool operator==(const AppearanceSettings &, const AppearanceSettings &) = default;
};

struct Settings
{
    QString character; // id of the one you play, see CharacterStore
    // The setup guide has been finished or skipped, see setupWanted().
    bool setupDone = false;
    TradeSettings trade;
    PobSettings pob;
    OverlaySettings overlay;
    AppearanceSettings appearance;

    friend bool operator==(const Settings &, const Settings &) = default;
};

// Anything missing or unreadable reads as its default, and numbers are held
// to their limits. Trade searches are on or off as a beta or a release starts
// them, until the file says what you chose.
Settings loadSettings(const QSettings &store, bool beta = isBetaBuild());
// Leaves out whether trade searches are on while it's what a beta or a release
// starts with and you haven't chosen, so a later version still starts them its
// own way.
void saveSettings(QSettings &store, const Settings &settings, bool beta = isBetaBuild());

} // namespace pou
