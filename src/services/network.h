// SPDX-License-Identifier: MIT
#pragma once

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

} // namespace pou
