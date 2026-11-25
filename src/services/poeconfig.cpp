// SPDX-License-Identifier: MIT
#include "services/poeconfig.h"

namespace pou {

PoeConfig builtInPoeConfig()
{
    return {.clientId = QString::fromUtf8(POU_POE_CLIENT_ID),
            .contact = QString::fromUtf8(POU_POE_CONTACT),
            .ports = parsePorts(QString::fromUtf8(POU_POE_REDIRECT_PORTS)),
            .tradeScope = QString::fromUtf8(POU_POE_TRADE_SCOPE),
            .tradeUrl = QString::fromUtf8(POU_POE_TRADE_URL)};
}

} // namespace pou
