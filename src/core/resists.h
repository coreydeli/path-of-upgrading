// SPDX-License-Identifier: MIT
#pragma once

#include "core/item.h"

#include <QDateTime>
#include <QJsonObject>
#include <array>
#include <cstddef>
#include <optional>

namespace pou {

// Your resistances, kept in step with your gear.
//
// What you enter, or import from Path of Building, is a snapshot taken with
// the gear you wore then. The record remembers how much of each resistance
// that gear gave, so when you swap an item later the totals move with it:
// live = entered - gear then + gear now.
//
// Path of Building's numbers can include things that aren't always on in game
// (charms, flasks, buffs, conditions ticked in its Configuration tab) and typed
// in numbers go stale, so a resistance only counts as safely capped when it's
// over the cap by a margin, the buffer.

enum class Element { Fire, Cold, Lightning, Chaos };
inline constexpr std::array allElements = {Element::Fire, Element::Cold, Element::Lightning, Element::Chaos};

QString elementName(Element e); // "Fire"
QString elementId(Element e);   // "fire"
QString resistKey(Element e);   // "#% to Fire Resistance"

template<typename T>
struct PerElement
{
    std::array<T, 4> values{};

    T &operator[](Element e) { return values[std::size_t(e)]; }
    const T &operator[](Element e) const { return values[std::size_t(e)]; }
    friend bool operator==(const PerElement &, const PerElement &) = default;
};

enum class ResistSource {
    Manual, // typed in
    Game,   // read off the in-game character sheet
    Pob,    // imported from Path of Building
};

double defaultBuffer(ResistSource source);

PerElement<double> itemResists(const Item &item);
// main slots only: weapon swap, flasks and jewels don't add up
PerElement<double> gearResists(const Equipped &equipped);
// "+% to Maximum Fire Resistance" and the like, from gear
PerElement<double> gearMaxResists(const Equipped &equipped);

// What you typed in or imported. Blank fields stay unknown.
struct ResistEntry
{
    PerElement<std::optional<double>> values;
    std::optional<double> cap;
    std::optional<double> buffer;
};

struct ResistRecord
{
    PerElement<std::optional<double>> entered;
    double cap = 75;
    double buffer = 10;
    bool bufferSet = false; // you chose the buffer, so a new snapshot keeps it
    ResistSource source = ResistSource::Manual;
    QDateTime at;
    PerElement<double> gear; // what the gear gave when this was taken

    friend bool operator==(const ResistRecord &, const ResistRecord &) = default;
};

ResistRecord snapshotResists(const ResistEntry &entry, const Equipped &equipped, ResistSource source,
                             const ResistRecord *previous = nullptr);

// The same item copied again with more detail (say a Mageblood now read with
// its Legacies) isn't a change to your character: shift the remembered gear
// so the totals don't move.
ResistRecord rebaseResists(ResistRecord record, const Item &oldItem, const Item &newItem);

// Your resistances with the gear you wear now.
struct LiveResists
{
    PerElement<std::optional<double>> values;
    PerElement<double> caps; // the cap plus maximum resistance from gear
    double cap = 75;
    double buffer = 0;
    ResistSource source = ResistSource::Pob;
    QDateTime at;
};

// Empty when no resistance was entered.
std::optional<LiveResists> liveResists(const ResistRecord &record, const Equipped &equipped);

QJsonObject toJson(const ResistRecord &record);
ResistRecord resistRecordFromJson(const QJsonObject &o);

} // namespace pou
