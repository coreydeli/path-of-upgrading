// SPDX-License-Identifier: MIT
#include "core/gearscore.h"

#include "core/numbers.h"
#include "core/slots.h"
#include "core/statnorm.h"

#include <QSet>
#include <algorithm>
#include <functional>
#include <numeric>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

double importanceOf(const ProfileStat &s, const QString &slot, const GearScoreOptions &options)
{
    return s.importance.at(slot) * options.tweaks.value(s.label, 1.0);
}

double capOf(const ProfileStat &s, const QString &slot)
{
    const double cap = s.cap.at(slot);
    return cap != 0 ? cap : 1;
}

bool isIgnored(const GearScoreOptions &options, const QString &key)
{
    return options.ignore && options.ignore(key);
}

std::optional<double> needFor(const ProfileStat &s, const GearScoreOptions &options)
{
    return s.resist ? options.resistNeed[*s.resist] : std::nullopt;
}

// How many stats an excellent item for the slot has.
qsizetype idealStatCount(const QString &slot)
{
    static const QStringList sevenStats = {
        u"Weapon"_s, u"Helmet"_s, u"Body Armour"_s, u"Gloves"_s, u"Boots"_s, u"Offhand"_s,
    };
    if (slot == u"Jewel")
        return 4;
    return sevenStats.contains(slot) ? 7 : 6;
}

QString resistNote(Element e, double need, bool partly)
{
    const QString name = elementName(e) + u" res: "_s;
    if (need <= 0)
        return name + u"you're already capped, so it doesn't count."_s;
    const QString more = formatNumber(roundHalfUp(need));
    if (partly)
        return name + u"you only need "_s + more + u" more to reach the cap, so the rest doesn't count."_s;
    return name + u"you only need "_s + more + u" more to reach the cap, so it isn't counted."_s;
}
} // namespace

std::optional<GearScore> gearScore(const Item &item, const GearScoreOptions &options)
{
    const BuildProfile *profile = buildProfile(options.build);
    if (!profile || item.slot.isEmpty())
        return std::nullopt;

    GearScore result;
    result.build = profile->id;
    result.buildName = profile->name;
    result.slot = groupOf(item.slot);
    const QString &slot = result.slot;
    if (slot == u"Charm" || slot == u"Life Flask" || slot == u"Mana Flask")
        result.unrated = GearScore::Unrated::FlaskOrCharm;
    else if (item.unidentified)
        result.unrated = GearScore::Unrated::Unidentified;
    else if (item.rarity.contains(u"unique", Qt::CaseInsensitive))
        result.unrated = GearScore::Unrated::Unique;
    if (!result.isRated())
        return result;

    QList<const ProfileStat *> wanted;
    for (const ProfileStat &s : profile->stats) {
        if (importanceOf(s, slot, options) > 0)
            wanted << &s;
    }
    // What the ideal item is made of. Ignored stats don't count towards it,
    // and nor do resistances you're nearly capped on.
    QList<const ProfileStat *> usable;
    for (const ProfileStat *s : std::as_const(wanted)) {
        if (!s->key.isEmpty() && isIgnored(options, s->key))
            continue;
        if (const auto need = needFor(*s, options); need && *need < s->cap.at(slot) * 0.25)
            continue;
        usable << s;
    }
    QList<double> importances;
    for (const ProfileStat *s : std::as_const(usable))
        importances << importanceOf(*s, slot, options);
    std::sort(importances.begin(), importances.end(), std::greater<>());
    const qsizetype idealCount = std::min(idealStatCount(slot), importances.size());
    double ideal = std::accumulate(importances.begin(), importances.begin() + idealCount, 0.0);
    if (ideal == 0)
        ideal = 1;

    QHash<const ProfileStat *, double> qualitySoFar;
    QSet<const ProfileStat *> used;
    QStringList notes;
    const auto note = [&notes](const QString &text) {
        if (!notes.contains(text))
            notes << text;
    };

    for (const StatSource &x : statSources(item)) {
        if (!(x.value > 0) || isIgnored(options, x.key))
            continue;
        const auto covers = [&x](const ProfileStat *s) {
            return s->pattern.match(x.key).hasMatch();
        };
        const auto entry = std::find_if(usable.cbegin(), usable.cend(), covers);
        if (entry == usable.cend()) {
            const auto capped = std::find_if(wanted.cbegin(), wanted.cend(), [&](const ProfileStat *s) {
                return s->resist && covers(s) && !usable.contains(s);
            });
            if (capped != wanted.cend()) {
                // without a need it was left out for being ignored, which says enough
                if (const auto need = needFor(**capped, options))
                    note(resistNote(*(*capped)->resist, *need, false));
                continue;
            }
            if ((x.origin == StatOrigin::Mod || x.origin == StatOrigin::Implicit) && !isInBase(item, x.key))
                result.wasted << x.key;
            continue;
        }

        const ProfileStat *s = *entry;
        double value = x.value;
        if (const auto need = needFor(*s, options); need && value > *need) {
            note(resistNote(*s->resist, *need, true));
            value = *need;
        }
        // One profile stat can't score more than about one excellent roll, even
        // from several mods (implicit and explicit life, or "+# to all
        // Attributes" counting as each attribute).
        const double soFar = qualitySoFar.value(s);
        const double quality = std::max(0.0, std::min(value / capOf(*s, slot), 1.15 - soFar));
        if (quality <= 0)
            continue;
        qualitySoFar[s] = soFar + quality;
        // Runes can be swapped out, and conditional mods aren't always on.
        const double factor =
            (x.origin == StatOrigin::Socket ? 0.5 : 1.0) * (isConditionalStat(x.key) ? 0.6 : 1.0);
        used.insert(s);
        result.contributions.append({x.key, s->label, x.value, int(roundHalfUp(quality * 100)),
                                     roundTo(importanceOf(*s, slot, options) * quality * factor, 1),
                                     x.origin});
    }

    std::stable_sort(
        result.contributions.begin(), result.contributions.end(),
        [](const GearContribution &a, const GearContribution &b) { return a.points > b.points; });
    const double total =
        std::accumulate(result.contributions.cbegin(), result.contributions.cend(), 0.0,
                        [](double sum, const GearContribution &c) { return sum + c.points; });
    result.score = std::clamp(int(roundHalfUp(100 * total / ideal)), 0, 100);
    result.grade = gearGrade(result.score);

    QList<const ProfileStat *> unused;
    for (const ProfileStat *s : std::as_const(usable)) {
        if (!used.contains(s))
            unused << s;
    }
    std::stable_sort(unused.begin(), unused.end(), [&](const ProfileStat *a, const ProfileStat *b) {
        return importanceOf(*a, slot, options) > importanceOf(*b, slot, options);
    });
    for (qsizetype i = 0; i < std::min<qsizetype>(3, unused.size()); ++i) {
        if (importanceOf(*unused[i], slot, options) >= 5)
            result.missing << unused[i]->label;
    }

    result.ideal = roundTo(ideal, 1);
    result.resistNote = notes.join(u' ');
    return result;
}

QString gearGrade(int score)
{
    if (score >= 85)
        return u"S"_s;
    if (score >= 70)
        return u"A"_s;
    if (score >= 55)
        return u"B"_s;
    if (score >= 40)
        return u"C"_s;
    return u"D"_s;
}

double usefulness(const QString &slot, const QString &key, const GearScoreOptions &options)
{
    const BuildProfile *profile = buildProfile(options.build);
    if (!profile)
        return 0;
    const QString group = groupOf(slot);
    for (const ProfileStat &s : profile->stats) {
        if (!s.pattern.match(key).hasMatch())
            continue;
        const double importance = importanceOf(s, group, options);
        return importance > 0 ? importance / capOf(s, group) : 0;
    }
    return 0;
}

QList<KeyStat> topStats(const QString &slot, int count, const GearScoreOptions &options)
{
    const BuildProfile *profile = buildProfile(options.build);
    if (!profile)
        return {};
    const QString group = groupOf(slot);
    QList<KeyStat> out;
    for (const ProfileStat &s : profile->stats) {
        if (s.key.isEmpty() || s.resist)
            continue;
        const double importance = importanceOf(s, group, options);
        if (importance >= 4)
            out.append({s.key, s.label, importance, s.cap.at(group)});
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const KeyStat &a, const KeyStat &b) { return a.importance > b.importance; });
    return out.mid(0, count);
}

QList<SearchWeight> searchWeights(const QString &slot, const GearScoreOptions &options)
{
    const BuildProfile *profile = buildProfile(options.build);
    if (!profile)
        return {};
    const QString group = groupOf(slot);
    QList<SearchWeight> out;
    for (const ProfileStat &s : profile->stats) {
        const double importance = importanceOf(s, group, options);
        if (!(importance > 0) || (!s.key.isEmpty() && isIgnored(options, s.key)))
            continue;
        const std::optional<double> need = needFor(s, options);
        if (need && *need <= 0)
            continue;
        const double cap = capOf(s, group);
        out.append({s.label, s.pattern, s.key, importance / cap, importance, cap, s.resist, need});
    }
    return out;
}

} // namespace pou
