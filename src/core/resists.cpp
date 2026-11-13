// SPDX-License-Identifier: MIT
#include "core/resists.h"

#include "core/numbers.h"
#include "core/slots.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QString sourceId(ResistSource source)
{
    switch (source) {
    case ResistSource::Manual:
        return u"manual"_s;
    case ResistSource::Game:
        return u"game"_s;
    case ResistSource::Pob:
        return u"pob"_s;
    }
    return {};
}

ResistSource sourceFromId(const QString &id)
{
    if (id == u"game")
        return ResistSource::Game;
    if (id == u"pob")
        return ResistSource::Pob;
    return ResistSource::Manual;
}

QJsonObject perElementJson(const PerElement<double> &values)
{
    QJsonObject o;
    for (Element e : allElements)
        o.insert(elementId(e), values[e]);
    return o;
}

} // namespace

QString elementName(Element e)
{
    switch (e) {
    case Element::Fire:
        return u"Fire"_s;
    case Element::Cold:
        return u"Cold"_s;
    case Element::Lightning:
        return u"Lightning"_s;
    case Element::Chaos:
        return u"Chaos"_s;
    }
    return {};
}

QString elementId(Element e)
{
    return elementName(e).toLower();
}

QString resistKey(Element e)
{
    return u"#% to "_s + elementName(e) + u" Resistance"_s;
}

double defaultBuffer(ResistSource source)
{
    switch (source) {
    case ResistSource::Pob:
        return 15;
    case ResistSource::Game:
        return 5;
    case ResistSource::Manual:
        break;
    }
    return 10;
}

PerElement<double> itemResists(const Item &item)
{
    PerElement<double> out;
    for (Element e : allElements)
        out[e] = item.stats.value(resistKey(e));
    return out;
}

PerElement<double> gearResists(const Equipped &equipped)
{
    PerElement<double> out;
    for (auto it = equipped.cbegin(); it != equipped.cend(); ++it) {
        if (!mainSlots().contains(it.key()))
            continue;
        const PerElement<double> r = itemResists(it.value());
        for (Element e : allElements)
            out[e] += r[e];
    }
    return out;
}

PerElement<double> gearMaxResists(const Equipped &equipped)
{
    PerElement<double> out;
    for (auto it = equipped.cbegin(); it != equipped.cend(); ++it) {
        if (!mainSlots().contains(it.key()))
            continue;
        const StatMap &stats = it.value().stats;
        for (Element e : allElements) {
            out[e] += stats.value(QString(u"#% to Maximum "_s + elementName(e) + u" Resistance"_s));
            if (e != Element::Chaos)
                out[e] += stats.value(u"#% to all Maximum Elemental Resistances");
        }
    }
    return out;
}

ResistRecord snapshotResists(const ResistEntry &entry, const Equipped &equipped, ResistSource source,
                             const ResistRecord *previous)
{
    ResistRecord r;
    r.entered = entry.values;
    for (Element e : allElements) {
        if (previous && previous->capSet[e]) {
            r.cap[e] = previous->cap[e];
            r.capSet[e] = true;
        } else if (entry.cap[e].value_or(0) > 0) {
            r.cap[e] = *entry.cap[e];
        } else if (previous) {
            r.cap[e] = previous->cap[e];
        }
    }
    r.source = source;
    r.at = QDateTime::currentDateTimeUtc();
    r.gear = gearResists(equipped);
    if (entry.buffer) {
        r.buffer = *entry.buffer;
        r.bufferSet = true;
    } else if (previous && previous->bufferSet) {
        r.buffer = previous->buffer;
        r.bufferSet = true;
    } else {
        r.buffer = defaultBuffer(source);
    }
    return r;
}

ResistRecord rebaseResists(ResistRecord record, const Item &oldItem, const Item &newItem)
{
    if (oldItem.name != newItem.name || oldItem.baseType != newItem.baseType)
        return record;
    const PerElement<double> before = itemResists(oldItem);
    const PerElement<double> after = itemResists(newItem);
    for (Element e : allElements)
        record.gear[e] += after[e] - before[e];
    return record;
}

std::optional<LiveResists> liveResists(const ResistRecord &record, const Equipped &equipped)
{
    LiveResists out;
    out.buffer = record.buffer;
    out.source = record.source;
    out.at = record.at;

    const PerElement<double> now = gearResists(equipped);
    const PerElement<double> max = gearMaxResists(equipped);
    bool any = false;
    for (Element e : allElements) {
        out.caps[e] = (record.cap[e] > 0 ? record.cap[e] : 75) + max[e];
        if (const std::optional<double> v = record.entered[e]) {
            out.values[e] = roundHalfUp(*v - record.gear[e] + now[e]);
            any = true;
        }
    }
    if (!any)
        return std::nullopt;
    return out;
}

QJsonObject toJson(const ResistRecord &record)
{
    QJsonObject o;
    for (Element e : allElements) {
        if (record.entered[e])
            o.insert(elementId(e), *record.entered[e]);
    }
    o.insert(u"cap", perElementJson(record.cap));
    QJsonObject set;
    for (Element e : allElements)
        set.insert(elementId(e), record.capSet[e]);
    o.insert(u"capSet", set);
    o.insert(u"buffer", record.buffer);
    o.insert(u"bufferSet", record.bufferSet);
    o.insert(u"source", sourceId(record.source));
    o.insert(u"at", record.at.toString(Qt::ISODateWithMs));
    o.insert(u"gear", perElementJson(record.gear));
    return o;
}

ResistRecord resistRecordFromJson(const QJsonObject &o)
{
    ResistRecord r;
    const QJsonObject gear = o.value(u"gear").toObject();
    for (Element e : allElements) {
        if (const QJsonValue v = o.value(elementId(e)); v.isDouble())
            r.entered[e] = v.toDouble();
        r.gear[e] = gear.value(elementId(e)).toDouble();
    }
    r.source = sourceFromId(o.value(u"source").toString());
    const QJsonValue cap = o.value(u"cap");
    if (cap.isDouble()) {
        // saved with one maximum for all of them, which they all take
        for (Element e : allElements) {
            r.cap[e] = cap.toDouble();
            r.capSet[e] = cap.toDouble() != 75;
        }
    } else {
        const QJsonObject caps = cap.toObject();
        const QJsonObject set = o.value(u"capSet").toObject();
        for (Element e : allElements) {
            r.cap[e] = caps.value(elementId(e)).toDouble(75);
            r.capSet[e] = set.value(elementId(e)).toBool();
        }
    }
    r.bufferSet = o.value(u"bufferSet").toBool();
    r.buffer = o.value(u"buffer").toDouble(defaultBuffer(r.source));
    r.at = QDateTime::fromString(o.value(u"at").toString(), Qt::ISODateWithMs);
    return r;
}

} // namespace pou
