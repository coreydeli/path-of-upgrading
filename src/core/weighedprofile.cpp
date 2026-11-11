// SPDX-License-Identifier: MIT
#include "core/weighedprofile.h"

#include "core/numbers.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

constexpr double smallest = 0.5;

struct Entry
{
    QString key;
    double importance = 0;
    double cap = 0;
};

std::optional<Element> resistOf(const QString &key)
{
    for (Element e : allElements) {
        if (key == resistKey(e))
            return e;
    }
    return std::nullopt;
}

// The group's entries, most important first, importance out of 10.
QList<Entry> entriesFor(const SlotWeighing &weighing, const SlotMods &mods)
{
    const QString itemClass = classOfSlot(weighing.group);
    // one stat can come from several mods
    QList<Entry> raw;
    for (const WeighedMod &m : weighing.mods) {
        if (m.key.isEmpty() || !std::isfinite(m.weight) || m.weight <= 0)
            continue;
        const auto top = itemClass.isEmpty() ? std::nullopt : mods.best(itemClass, m.key);
        const double cap = top ? top->high : m.top;
        if (!(cap > 0))
            continue;
        const double worth = m.weight * cap;
        const auto same = std::ranges::find(raw, m.key, &Entry::key);
        if (same == raw.end())
            raw.append({m.key, worth, cap});
        else if (worth > same->importance)
            *same = {m.key, worth, cap};
    }
    double biggest = 0;
    for (const Entry &e : std::as_const(raw))
        biggest = std::max(biggest, e.importance);

    QList<Entry> out;
    for (Entry e : std::as_const(raw)) {
        e.importance = roundTo(e.importance * 10 / biggest, 1);
        if (e.importance >= smallest)
            out.append(e);
    }

    if (out.isEmpty())
        return out;
    // which the resistances you need would otherwise lose
    if (!itemClass.isEmpty()) {
        for (Element e : allElements) {
            const auto top = mods.best(itemClass, resistKey(e));
            if (!top)
                continue;
            const auto there = std::ranges::find(out, resistKey(e), &Entry::key);
            if (there == out.end())
                out.append({resistKey(e), resistFloor, top->high});
            else if (there->importance < resistFloor)
                there->importance = resistFloor;
        }
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const Entry &a, const Entry &b) { return a.importance > b.importance; });
    return out;
}

QJsonObject perSlotToJson(const PerSlot &value)
{
    QJsonObject o;
    for (const QString &group : ratedGroups()) {
        if (value.at(group) != 0)
            o.insert(group, value.at(group));
    }
    return o;
}

PerSlot perSlotFromJson(const QJsonObject &o)
{
    QList<std::pair<QString, double>> overrides;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        if (it.value().isDouble())
            overrides.append({it.key(), it.value().toDouble()});
    }
    return PerSlot::of(overrides);
}

} // namespace

WeighedProfile weighedProfile(const QList<SlotWeighing> &weighings, const BuildProfile &start,
                              const SlotMods &mods)
{
    WeighedProfile out;
    out.profile = {.id = weighedProfileId.toString(),
                   .name = u"Weighed by Path of Building"_s,
                   .summary = u"What Path of Building says each stat is worth to your build in each slot."_s,
                   .stats = {}};

    QHash<QString, QString> startLabels;
    for (const ProfileStat &s : start.stats) {
        if (!s.key.isEmpty() && !startLabels.contains(s.key))
            startLabels.insert(s.key, s.label);
    }

    // by key, in the order they first come
    QStringList keys;
    QHash<QString, QList<std::pair<QString, double>>> importance;
    QHash<QString, QList<std::pair<QString, double>>> caps;
    for (const SlotWeighing &weighing : weighings) {
        const QList<Entry> entries = entriesFor(weighing, mods);
        if (entries.isEmpty() || out.slots.contains(weighing.group))
            continue;
        out.slots << weighing.group;
        for (const Entry &e : entries) {
            if (!importance.contains(e.key))
                keys << e.key;
            importance[e.key].append({weighing.group, e.importance});
            caps[e.key].append({weighing.group, e.cap});
        }
    }

    for (const QString &key : std::as_const(keys)) {
        const std::optional<Element> resist = resistOf(key);
        const QString label = resist ? resistLabel(*resist) : startLabels.value(key, key);
        out.profile.stats.append(
            {.label = label,
             .pattern = QRegularExpression(u'^' + QRegularExpression::escape(key) + u'$'),
             .importance = PerSlot::of(importance.value(key)),
             .cap = PerSlot::of(caps.value(key)),
             .key = key,
             .resist = resist});
    }
    auto &stats = out.profile.stats;
    const auto best = [](const ProfileStat &s) {
        double most = 0;
        for (const auto &[group, value] : s.importance.overrides())
            most = std::max(most, value);
        return most;
    };
    std::stable_sort(stats.begin(), stats.end(),
                     [&best](const ProfileStat &a, const ProfileStat &b) { return best(a) > best(b); });
    return out;
}

QList<ProfileStat> itemPropertyStats(const BuildProfile &start)
{
    QList<ProfileStat> out;
    std::ranges::copy_if(start.stats, std::back_inserter(out), isPropertyStat);
    return out;
}

QString buildFingerprint(const QString &pobXml)
{
    return pobXml.isEmpty()
               ? QString()
               : QString::fromLatin1(
                     QCryptographicHash::hash(pobXml.toUtf8(), QCryptographicHash::Sha1).toHex());
}

QJsonObject toJson(const WeighedProfile &weighed)
{
    QJsonArray stats;
    for (const ProfileStat &s : weighed.profile.stats) {
        QJsonObject o = {
            {u"label"_s, s.label},
            {u"key"_s, s.key},
            {u"importance"_s, perSlotToJson(s.importance)},
            {u"cap"_s, perSlotToJson(s.cap)},
        };
        if (s.resist)
            o.insert(u"resist"_s, elementId(*s.resist));
        stats.append(o);
    }
    return {
        {u"pobVersion"_s, weighed.pobVersion},
        {u"date"_s, weighed.date},
        {u"damageWeight"_s, weighed.damageWeight},
        {u"build"_s, weighed.build},
        {u"slots"_s, QJsonArray::fromStringList(weighed.slots)},
        {u"stats"_s, stats},
    };
}

std::optional<WeighedProfile> weighedProfileFromJson(const QJsonObject &o)
{
    if (!o.value(u"stats").isArray() || !o.value(u"slots").isArray())
        return std::nullopt;
    WeighedProfile out;
    out.profile = {.id = weighedProfileId.toString(),
                   .name = u"Weighed by Path of Building"_s,
                   .summary = u"What Path of Building says each stat is worth to your build in each slot."_s,
                   .stats = {}};
    out.pobVersion = o.value(u"pobVersion").toString();
    out.date = o.value(u"date").toString();
    out.damageWeight = o.value(u"damageWeight").toDouble();
    out.build = o.value(u"build").toString();
    for (const QJsonValue &slot : o.value(u"slots").toArray()) {
        if (ratedGroups().contains(slot.toString()))
            out.slots << slot.toString();
    }
    for (const QJsonValue &v : o.value(u"stats").toArray()) {
        const QJsonObject s = v.toObject();
        const QString key = s.value(u"key").toString();
        const QString label = s.value(u"label").toString();
        if (key.isEmpty() || label.isEmpty())
            continue;
        std::optional<Element> resist;
        for (Element e : allElements) {
            if (s.value(u"resist").toString() == elementId(e))
                resist = e;
        }
        out.profile.stats.append(
            {.label = label,
             .pattern = QRegularExpression(u'^' + QRegularExpression::escape(key) + u'$'),
             .importance = perSlotFromJson(s.value(u"importance").toObject()),
             .cap = perSlotFromJson(s.value(u"cap").toObject()),
             .key = key,
             .resist = resist});
    }
    return out;
}

} // namespace pou
