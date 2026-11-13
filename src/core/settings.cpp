// SPDX-License-Identifier: MIT
#include "core/settings.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString colorThemeId(ColorTheme theme)
{
    return theme == ColorTheme::Desktop ? u"desktop"_s : u"app"_s;
}

ColorTheme colorThemeFromId(QStringView id)
{
    return id == u"desktop" ? ColorTheme::Desktop : ColorTheme::App;
}

QString decorationsId(Decorations decorations)
{
    return decorations == Decorations::Plain ? u"plain"_s : u"full"_s;
}

Decorations decorationsFromId(QStringView id)
{
    return id == u"plain" ? Decorations::Plain : Decorations::Full;
}

bool isBetaBuild()
{
    return POU_BETA_BUILD != 0;
}

QString tradeUnavailable(const TradeSettings &settings)
{
    if (!settings.enabled)
        return u"Trade searches are turned off. Turn them on with Search the trade site, on the Trade page."_s;
    if (!settings.league.canTrade())
        return u"There's no trading in solo self-found, so trade searches are off."_s;
    return {};
}

} // namespace pou
