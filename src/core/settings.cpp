// SPDX-License-Identifier: MIT
#include "core/settings.h"

#include <QSettings>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

constexpr double noLimit = std::numeric_limits<double>::max();

QString positionId(PopupPosition position)
{
    switch (position) {
    case PopupPosition::Cursor:
        return u"cursor"_s;
    case PopupPosition::Left:
        return u"left"_s;
    case PopupPosition::Right:
        return u"right"_s;
    case PopupPosition::Top:
        return u"top"_s;
    }
    return {};
}

PopupPosition positionFromId(QStringView id)
{
    for (PopupPosition position : {PopupPosition::Left, PopupPosition::Right, PopupPosition::Top}) {
        if (id == positionId(position))
            return position;
    }
    return PopupPosition::Cursor;
}

// Each leaves the value as it is when the key is missing.
void read(const QSettings &store, const QString &key, bool &value)
{
    value = store.value(key, value).toBool();
}

void read(const QSettings &store, const QString &key, QString &value)
{
    value = store.value(key, value).toString();
}

void read(const QSettings &store, const QString &key, double &value, double low, double high)
{
    bool ok = false;
    const double n = store.value(key).toDouble(&ok);
    if (ok && std::isfinite(n))
        value = std::clamp(n, low, high);
}

void read(const QSettings &store, const QString &key, int &value, int low, int high)
{
    bool ok = false;
    const int n = store.value(key).toInt(&ok);
    if (ok)
        value = std::clamp(n, low, high);
}

} // namespace

QString tradeUnavailable(const TradeSettings &settings)
{
    if (!settings.enabled)
        return u"Trade searches are turned off in the settings."_s;
    if (!settings.league.canTrade())
        return u"There's no trading in solo self-found, so trade searches are off."_s;
    return {};
}

Settings loadSettings(const QSettings &store)
{
    Settings s;
    read(store, u"character"_s, s.character);

    TradeSettings &trade = s.trade;
    read(store, u"trade/enabled"_s, trade.enabled);
    read(store, u"trade/league"_s, trade.league.name);
    read(store, u"trade/hardcore"_s, trade.league.hardcore);
    read(store, u"trade/ssf"_s, trade.league.soloSelfFound);
    trade.status = listingStatusFromId(store.value(u"trade/status"_s).toString());
    read(store, u"trade/budgetMin"_s, trade.budget.min, 0, noLimit);
    read(store, u"trade/budgetMax"_s, trade.budget.max, 0, noLimit);
    trade.budget.currency = currencyFromId(store.value(u"trade/currency"_s).toString());
    read(store, u"trade/divineRate"_s, trade.budget.divineRate, 0, noLimit);
    read(store, u"trade/autoRate"_s, trade.autoRate);

    PobSettings &pob = s.pob;
    read(store, u"pob/enabled"_s, pob.enabled);
    read(store, u"pob/location"_s, pob.location);
    read(store, u"pob/damageWeight"_s, pob.damageWeight, 0, 1);
    read(store, u"pob/resistsFirst"_s, pob.priorities.resistsFirst);
    read(store, u"pob/chaos"_s, pob.priorities.chaos);
    read(store, u"pob/keepDamage"_s, pob.priorities.keepDamage);
    read(store, u"pob/spirit"_s, pob.priorities.spirit);
    read(store, u"pob/attributes"_s, pob.priorities.attributes);

    OverlaySettings &overlay = s.overlay;
    read(store, u"overlay/hotkey"_s, overlay.hotkey);
    read(store, u"overlay/resultsHotkey"_s, overlay.resultsHotkey);
    read(store, u"overlay/advancedCopy"_s, overlay.advancedCopy);
    read(store, u"overlay/watchClipboard"_s, overlay.watchClipboard);
    overlay.position = positionFromId(store.value(u"overlay/position"_s).toString());
    read(store, u"overlay/scale"_s, overlay.scale, 0.5, 2);
    read(store, u"overlay/compact"_s, overlay.compact);
    read(store, u"overlay/colorblind"_s, overlay.colorblind);
    read(store, u"overlay/autoHideSeconds"_s, overlay.autoHideSeconds, 2, 120);
    read(store, u"overlay/paused"_s, overlay.paused);
    return s;
}

void saveSettings(QSettings &store, const Settings &s)
{
    store.setValue(u"character"_s, s.character);

    const TradeSettings &trade = s.trade;
    store.setValue(u"trade/enabled"_s, trade.enabled);
    store.setValue(u"trade/league"_s, trade.league.name);
    store.setValue(u"trade/hardcore"_s, trade.league.hardcore);
    store.setValue(u"trade/ssf"_s, trade.league.soloSelfFound);
    store.setValue(u"trade/status"_s, listingStatusId(trade.status));
    store.setValue(u"trade/budgetMin"_s, trade.budget.min);
    store.setValue(u"trade/budgetMax"_s, trade.budget.max);
    store.setValue(u"trade/currency"_s, currencyId(trade.budget.currency));
    store.setValue(u"trade/divineRate"_s, trade.budget.divineRate);
    store.setValue(u"trade/autoRate"_s, trade.autoRate);

    const PobSettings &pob = s.pob;
    store.setValue(u"pob/enabled"_s, pob.enabled);
    store.setValue(u"pob/location"_s, pob.location);
    store.setValue(u"pob/damageWeight"_s, pob.damageWeight);
    store.setValue(u"pob/resistsFirst"_s, pob.priorities.resistsFirst);
    store.setValue(u"pob/chaos"_s, pob.priorities.chaos);
    store.setValue(u"pob/keepDamage"_s, pob.priorities.keepDamage);
    store.setValue(u"pob/spirit"_s, pob.priorities.spirit);
    store.setValue(u"pob/attributes"_s, pob.priorities.attributes);

    const OverlaySettings &overlay = s.overlay;
    store.setValue(u"overlay/hotkey"_s, overlay.hotkey);
    store.setValue(u"overlay/resultsHotkey"_s, overlay.resultsHotkey);
    store.setValue(u"overlay/advancedCopy"_s, overlay.advancedCopy);
    store.setValue(u"overlay/watchClipboard"_s, overlay.watchClipboard);
    store.setValue(u"overlay/position"_s, positionId(overlay.position));
    store.setValue(u"overlay/scale"_s, overlay.scale);
    store.setValue(u"overlay/compact"_s, overlay.compact);
    store.setValue(u"overlay/colorblind"_s, overlay.colorblind);
    store.setValue(u"overlay/autoHideSeconds"_s, overlay.autoHideSeconds);
    store.setValue(u"overlay/paused"_s, overlay.paused);
}

} // namespace pou
