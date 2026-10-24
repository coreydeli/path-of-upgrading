// SPDX-License-Identifier: MIT
#pragma once

#include "app/tradefinder.h"

#include <QString>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>
#include <chrono>

namespace pou {

// What the popup says of a trade search for the item you checked.
struct TradeReport
{
    Q_GADGET
    QML_VALUE_TYPE(tradeReport)

    Q_PROPERTY(bool asked MEMBER asked)
    Q_PROPERTY(bool busy MEMBER busy)
    Q_PROPERTY(bool failed MEMBER failed)
    Q_PROPERTY(QString title MEMBER title)
    Q_PROPERTY(QString status MEMBER status)
    Q_PROPERTY(QString note MEMBER note)
    Q_PROPERTY(QUrl url MEMBER url)

public:
    bool asked = false; // whether there's been one for the item
    bool busy = false;  // and it's still going
    bool failed = false;
    QString title; // "Same stats or better than Doom Visage"
    // How many it found and where, "12 listed in Runes of Aldur", how it's
    // getting on, or why it found nothing.
    QString status;
    QString note; // how it had to search
    QUrl url;     // the listings, to open on the trade site

    friend bool operator==(const TradeReport &, const TradeReport &) = default;
};

// While it searches, after waiting that long for its turn.
TradeReport searchingReport(std::chrono::milliseconds wait = {});
TradeReport reportOf(const TradeFinder::Found &found);
TradeReport failedReport(const QString &why);

} // namespace pou
