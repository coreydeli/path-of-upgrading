// SPDX-License-Identifier: MIT
#pragma once

#include "core/poeoauth.h"

#include <QByteArray>
#include <QNetworkRequest>
#include <QUrl>

namespace pou {

// The app's User-Agent, as the sites it asks would like to know who's asking
// and where to find out more:
// "PathOfUpgrading/0.1.0 (+https://github.com/coreydeli/path-of-upgrading)"
QByteArray userAgent();

// A request to one of the JSON APIs the app uses.
QNetworkRequest apiRequest(const QUrl &url);

// The User-Agent Path of Exile's API asks apps to send, with the client id,
// the version and a contact:
// "OAuth examplepou/0.9.0 (contact: example.invalid) (+https://example.invalid/app)"
QByteArray poeUserAgent(const PoeConfig &config);

// A request to Path of Exile's API or its OAuth service.
QNetworkRequest poeRequest(const QUrl &url, const PoeConfig &config);

} // namespace pou
