// SPDX-License-Identifier: MIT
#pragma once

#include "core/seenstats.h"
#include "core/tradedata.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace pou {

// One stat in the stat picker, merged from every version the trade site has
// of it: the explicit mod, the pseudo total, the rune... StatIndex::match()
// finds the version to search for.
struct CatalogEntry
{
    QString text;             // the wording closest to how items put it
    QString normalized;       // normalizeStat() of the text, the same for every version
    QStringList types;        // the trade stat types it comes in, in the site's order
    QHash<QString, int> seen; // item class -> how many of your copied items had it

    // On items of one class, or of any class when it's empty.
    int seenOn(const QString &itemClass = {}) const;
};

struct StatCatalog
{
    QList<CatalogEntry> entries; // alphabetical
    QStringList classes;         // the item classes you've copied, sorted
};

// Every stat the trade site knows, apart from sanctum ones and counts like
// "# Prefix Modifiers", which say nothing about what an item does.
StatCatalog buildCatalog(const StatIndex &stats, const SeenStats &seen);

// What the picker calls a stat type: "Total" for pseudo stats, "Rune" for
// augments, "Explicit"...
QString statTypeLabel(const QString &type);

// The key the app weights for a trade stat: "+#% to Fire Resistance" is
// "#% to Fire Resistance".
QString itemKeyFromText(const QString &text);

} // namespace pou
