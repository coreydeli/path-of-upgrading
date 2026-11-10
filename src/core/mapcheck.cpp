// SPDX-License-Identifier: MIT
#include "core/mapcheck.h"

#include "core/numbers.h"

#include <QLocale>
#include <QRegularExpression>
#include <algorithm>
#include <limits>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

constexpr double never = std::numeric_limits<double>::infinity();

// What of your build makes a mod worse.
enum class Need {
    Nothing,
    ElementRes,    // the element the rule captures
    AnyElementRes, // any of the three
    ChaosRes,
    Health, // a thin pool of life and energy shield
};

struct CostRule
{
    QRegularExpression pattern;
    QString costs;
    Danger base = Danger::Mild;
    // the first number in the line, from where it's a step up
    double riskyAt = never;
    double deadlyAt = never;
    Need need = Need::Nothing;
};

struct GainRule
{
    QRegularExpression pattern;
    QString text; // %1 is the first number in the line
};

QRegularExpression rx(const QString &pattern)
{
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
}

// The mods are from poe2db.tw: the Waystones Mods list on its Waystones page
// and the Precursor Towers Mods list on its Tablet page, as of November 2026.
// The danger of each is ours, read from what the mod does.
const QList<CostRule> &costRules()
{
    static const QList<CostRule> rules = {
        {rx(uR"(Monsters deal (\d+)% of Damage as Extra (?:Fire|Cold|Lightning))"_s),
         u"Monsters add extra elemental damage to every hit"_s, Danger::Mild, 10, 20, Need::ElementRes},
        {rx(uR"(Monsters deal (\d+)% of Damage as Extra Chaos)"_s),
         u"Monsters add extra chaos damage to every hit"_s, Danger::Mild, 10, 15, Need::ChaosRes},
        {rx(uR"((\d+)% less Recovery Rate of Life and Energy Shield)"_s),
         u"You recover much less life and energy shield, so healing and regeneration fall short"_s,
         Danger::Risky, 20, 30},
        {rx(uR"((-?\d+)% maximum Player Resistances)"_s),
         u"Your resistances can't reach their usual cap, so every hit does more"_s, Danger::Deadly},
        {rx(uR"(Monster Damage Penetrates (\d+)% Elemental Resistances)"_s),
         u"Monster hits ignore part of your elemental resistances"_s, Danger::Risky, 10, 14,
         Need::AnyElementRes},
        {rx(uR"((\d+)% increased Monster Damage)"_s), u"Monsters hit harder"_s, Danger::Mild, 15, never,
         Need::Health},
        {rx(uR"(Monsters have (\d+)% increased Attack, Cast and Movement Speed)"_s),
         u"Monsters attack, cast and move faster"_s, Danger::Risky, 0, never, Need::Health},
        {rx(uR"(Monsters have (\d+)% increased Critical Hit Chance)"_s), u"Monsters crit far more often"_s,
         Danger::Mild, 200, never, Need::Health},
        {rx(uR"((\d+)% more Monster Life)"_s), u"Monsters take longer to kill"_s, Danger::Mild, 20},
        {rx(uR"(\+(\d+)% Monster Elemental Resistances)"_s),
         u"Monsters resist elemental damage, so you kill them slower"_s, Danger::Mild, 35},
        {rx(uR"(Monsters fire (\d+) additional Projectiles)"_s), u"Monsters fire more projectiles at you"_s,
         Danger::Risky, 0, never, Need::Health},
        {rx(uR"(Monsters have (\d+)% chance to Poison on Hit)"_s), u"Monsters poison you"_s},
        {rx(uR"(Monsters have (\d+)% chance to inflict Bleeding on Hit)"_s), u"Monsters make you bleed"_s,
         Danger::Mild, 20},
        {rx(uR"(Monsters Break Armour equal to (\d+)% of Physical Damage dealt)"_s),
         u"Monsters strip your armour"_s, Danger::Mild, 30},
        {rx(uR"(Monsters have (\d+)% increased Accuracy Rating)"_s), u"Monsters hit you more often"_s},
        {rx(uR"(Monsters have (\d+)% increased Stun Buildup)"_s), u"Monsters stun you more easily"_s},
        {rx(uR"(Monsters? have (\d+)% increased Elemental Ailment Application)"_s),
         u"Monsters freeze, shock and ignite you more easily"_s, Danger::Mild, 100},
        {rx(uR"(Monsters have (\d+)% increased (?:Ailment|Stun) Threshold)"_s),
         u"Monsters are harder to stun and to ail"_s},
        {rx(uR"(Monsters are (?:Armoured|Evasive))"_s), u"Monsters are tougher to hit or to hurt"_s},
        {rx(uR"(Monsters gain (\d+)% of maximum Life as Extra maximum Energy Shield)"_s),
         u"Monsters have extra energy shield"_s},
        {rx(uR"(Monsters have (\d+)% more Effectiveness)"_s), u"Monsters are tougher and hit harder"_s},
        {rx(uR"(Monsters have (\d+)% increased Effectiveness)"_s), u"Monsters are tougher and hit harder"_s},
        {rx(uR"(Players gain (\d+)% reduced Flask Charges)"_s), u"Your flasks fill slower"_s, Danger::Mild,
         30},
        {rx(uR"((\d+)% less Cooldown Recovery Rate)"_s), u"Your skills cool down slower"_s, Danger::Mild, 25},
        {rx(uR"(Players are periodically Cursed with)"_s),
         u"You are cursed over and over, which slows or weakens you"_s, Danger::Risky},
        {rx(uR"(Area has patches of (?:Ignited|Chilled|Shocked) Ground)"_s),
         u"Harmful ground patches on the floor"_s},
        {rx(uR"(Monsters have (\d+)% increased Area of Effect)"_s), u"Monster attacks are wider"_s},
        {rx(uR"(Monsters take (\d+)% reduced Extra Damage from Critical Hits)"_s),
         u"Your critical hits do less, which hurts crit builds"_s},
        {rx(uR"((\d+)% less effect of Curses on Monsters)"_s), u"Your curses work worse"_s},
        {rx(uR"(Rare Monsters have (\d+) additional Modifiers)"_s), u"Rare monsters get another mod"_s},
        {rx(uR"(Players and their Minions deal no damage for)"_s),
         u"You and your minions deal no damage for part of every ten seconds"_s, Danger::Deadly},
        {rx(uR"(Players are Marked for Death)"_s),
         u"You take much more damage after killing a rare or unique monster"_s, Danger::Risky},
        {rx(uR"((\d+)% less Movement and Skill Speed for each time)"_s),
         u"You get slower the more skills you use"_s, Danger::Risky},
        {rx(uR"(Monsters inflict (\d+) Grasping Vines on Hit)"_s), u"Monsters root and slow you"_s},
        {rx(uR"(Mana Siphoning Ground)"_s), u"Ground that drains your mana"_s},
        // tablets
        {rx(uR"((\d+)% reduced Pack Size in Map)"_s), u"Fewer monsters in the map"_s},
        {rx(uR"(Map has (\d+) additional random Modifiers)"_s), u"The map gets extra random mods"_s,
         Danger::Risky},
        {rx(uR"(Unique Monsters have (\d+) additional Rare Modifiers)"_s),
         u"Unique monsters get extra mods"_s, Danger::Risky},
        {rx(uR"(Surpassing chance to have an additional Modifier)"_s),
         u"Rare monsters often get an extra mod"_s, Danger::Risky},
        {rx(uR"(Favours at Ritual Altars in Area costs (\d+)% increased Tribute)"_s),
         u"Ritual favours cost more tribute"_s},
        {rx(uR"(Delirium Fog .*(?:slower|additional seconds|Deliriousness))"_s),
         u"The delirium fog is harder to get rid of"_s},
        {rx(uR"(Abyssal Monsters have \(?(\d+)\)?% increased Effectiveness)"_s),
         u"Abyssal monsters are tougher"_s},
    };
    return rules;
}

const QList<GainRule> &gainRules()
{
    static const QList<GainRule> rules = {
        {rx(uR"((\d+)% (?:more|increased) (?:Quantity of )?Waystones (?:found|dropped))"_s),
         u"%1% more Waystones found"_s},
        {rx(uR"((\d+)% (?:more|increased) Rarity of Items)"_s), u"%1% more item rarity"_s},
        {rx(uR"((\d+)% (?:more|increased) Quantity of Items)"_s), u"%1% more item quantity"_s},
        {rx(uR"((\d+)% (?:more|increased) Pack size)"_s), u"%1% more pack size"_s},
        {rx(uR"((\d+)% (?:more|increased) (?:Magic and Rare Monsters|number of Rare Monsters|Magic Monsters))"_s),
         u"%1% more magic and rare monsters"_s},
        {rx(uR"(Rare Monsters have (\d+)% more chance of Monster Modifiers)"_s),
         u"%1% more chance of rare monster mods"_s},
        {rx(uR"((\d+)% increased Gold found)"_s), u"%1% more gold"_s},
        {rx(uR"((\d+)% increased Experience gain)"_s), u"%1% more experience"_s},
    };
    return rules;
}

// the game's own lines, as the rules read them
QString cleanLine(QString line)
{
    static const QRegularExpression range(uR"(\((-?\d+(?:\.\d+)?)[-–](-?\d+(?:\.\d+)?)\))"_s);
    static const QRegularExpression unscalable(uR"(\s+[—–-]+\s*Unscalable Value\s*$)"_s);
    static const QRegularExpression tag(uR"(\s*\((?:augmented|desecrated|fractured)\)\s*$)"_s);
    line.replace(range, QString()).replace(unscalable, QString()).replace(tag, QString());
    return line.trimmed();
}

struct RawMod
{
    QStringList lines;
    AffixSide side = AffixSide::Unknown;
};

bool isGain(const QString &line)
{
    return std::ranges::any_of(gainRules(),
                               [&](const GainRule &r) { return r.pattern.match(line).hasMatch(); });
}

// The mods are the blocks after the item level. Advanced copies put a header
// above each one, which also says prefix or suffix.
QList<RawMod> rawModsOf(const Item &item, MapKind kind)
{
    const QStringList blocks = item.raw.split(u"--------"_s);
    qsizetype first = -1;
    for (qsizetype b = 0; b < blocks.size(); ++b) {
        if (blocks.at(b).contains(u"Item Level:"_s)) {
            first = b + 1;
            break;
        }
    }
    QList<RawMod> mods;
    if (first < 0)
        return mods;

    bool headers = false;
    for (qsizetype b = first; b < blocks.size(); ++b) {
        for (const QString &raw : blocks.at(b).split(u'\n')) {
            const QString text = raw.trimmed();
            if (text.isEmpty())
                continue;
            if (text.startsWith(u'{')) {
                headers = true;
                RawMod mod;
                if (text.contains(u"Prefix Modifier"_s, Qt::CaseInsensitive))
                    mod.side = AffixSide::Prefix;
                else if (text.contains(u"Suffix Modifier"_s, Qt::CaseInsensitive))
                    mod.side = AffixSide::Suffix;
                mods << mod;
                continue;
            }
            const QString line = cleanLine(text);
            if (line.isEmpty() || line == u"Corrupted"_s || line == u"Unidentified"_s ||
                line == u"Mirrored"_s)
                continue;
            if (headers) {
                if (!mods.isEmpty())
                    mods.last().lines << line;
            } else if (kind == MapKind::Waystone && !mods.isEmpty() && isGain(line)) {
                // a waystone mod's rewards come on the lines after it
                mods.last().lines << line;
            } else {
                mods << RawMod{.lines = {line}};
            }
        }
    }
    mods.removeIf([](const RawMod &m) { return m.lines.isEmpty(); });
    return mods;
}

Danger stepUp(Danger d)
{
    return d == Danger::Deadly ? d : Danger(int(d) + 1);
}

QString percentOf(double v)
{
    return formatNumber(roundTo(v, 1)) + u'%';
}

// What your build says about a mod, empty when it makes no difference.
QString buildNoteFor(Need need, const QString &line, const MapBuild &build)
{
    static const QRegularExpression element(uR"((Fire|Cold|Lightning))"_s);
    const auto under = [&](Element e) -> std::optional<double> {
        if (!build.resists || !build.resists->values[e])
            return std::nullopt;
        const double have = *build.resists->values[e];
        if (have >= build.resists->caps[e])
            return std::nullopt;
        return have;
    };
    switch (need) {
    case Need::Nothing:
        return {};
    case Need::ElementRes: {
        const auto m = element.match(line);
        if (!m.hasMatch())
            return {};
        const Element e = m.captured(1).toLower() == u"fire"_s   ? Element::Fire
                          : m.captured(1).toLower() == u"cold"_s ? Element::Cold
                                                                 : Element::Lightning;
        if (const auto have = under(e))
            return u"Your %1 resistance is %2, under the cap."_s.arg(elementId(e), percentOf(*have));
        return {};
    }
    case Need::AnyElementRes: {
        QStringList names;
        for (const Element e : {Element::Fire, Element::Cold, Element::Lightning}) {
            if (under(e))
                names << elementId(e);
        }
        if (names.isEmpty())
            return {};
        return u"Your %1 resistance is under the cap."_s.arg(names.join(u", "_s));
    }
    case Need::ChaosRes:
        if (const auto have = under(Element::Chaos))
            return u"Your chaos resistance is %1, under the cap."_s.arg(percentOf(*have));
        return {};
    case Need::Health:
        if (build.health > 0 && build.health < 3000) {
            static const QLocale english(QLocale::English);
            return u"Your life and energy shield come to %1, which is low for this."_s.arg(
                english.toString(qlonglong(build.health)));
        }
        return {};
    }
    return {};
}

MapMod rate(const RawMod &raw, const MapBuild &build)
{
    MapMod mod;
    mod.lines = raw.lines;
    mod.side = raw.side;
    QStringList costs;
    bool anyCost = false;
    bool anyGain = false;
    bool unknown = false;
    for (const QString &line : raw.lines) {
        bool matched = false;
        for (const CostRule &rule : costRules()) {
            const auto m = rule.pattern.match(line);
            if (!m.hasMatch())
                continue;
            matched = anyCost = true;
            double value = 0;
            if (m.lastCapturedIndex() >= 1)
                value = std::abs(m.captured(1).toDouble());
            Danger d = rule.base;
            if (value >= rule.deadlyAt)
                d = Danger::Deadly;
            else if (value >= rule.riskyAt)
                d = std::max(d, Danger::Risky);
            if (!costs.contains(rule.costs))
                costs << rule.costs;
            mod.baseDanger = std::max(mod.baseDanger, d);
            if (const QString note = buildNoteFor(rule.need, line, build); !note.isEmpty()) {
                if (mod.buildNote.isEmpty())
                    mod.buildNote = note;
            }
            break;
        }
        if (matched)
            continue;
        for (const GainRule &rule : gainRules()) {
            const auto m = rule.pattern.match(line);
            if (!m.hasMatch())
                continue;
            matched = anyGain = true;
            mod.gives << rule.text.arg(formatNumber(roundTo(m.captured(1).toDouble(), 1)));
            break;
        }
        if (!matched)
            unknown = true;
    }
    mod.costs = costs.join(u", "_s);
    mod.danger = mod.baseDanger;
    if (!mod.buildNote.isEmpty())
        mod.danger = stepUp(mod.baseDanger);
    if (unknown && !anyCost && !anyGain) {
        mod.known = false;
        mod.costs = u"This mod isn't one the app knows yet, so it isn't rated."_s;
    }
    return mod;
}

QString nameOf(const MapMod &mod)
{
    return mod.lines.value(0);
}

} // namespace

int MapCheck::badMods() const
{
    return int(std::ranges::count_if(mods, [](const MapMod &m) { return m.isBad(); }));
}

QString dangerName(Danger danger)
{
    switch (danger) {
    case Danger::Harmless:
        return u"Harmless"_s;
    case Danger::Mild:
        return u"Mild"_s;
    case Danger::Risky:
        return u"Risky"_s;
    case Danger::Deadly:
        return u"Can end the run"_s;
    }
    Q_UNREACHABLE_RETURN({});
}

QString oddsText(int bad, int total)
{
    return u"%1 in %2"_s.arg(bad).arg(total);
}

std::optional<MapKind> mapKindOf(const Item &item)
{
    const QString c = item.itemClass.trimmed().toLower();
    if (c == u"waystones"_s || c == u"waystone"_s)
        return MapKind::Waystone;
    if (c.contains(u"tablet"_s))
        return MapKind::Tablet;
    return std::nullopt;
}

namespace {

QStringList suggest(const MapCheck &check)
{
    const int total = int(check.mods.size());
    const int bad = check.badMods();
    if (bad == 0)
        return {u"Run it as it is. Nothing on it is bad enough to be worth removing."_s};
    if (check.corrupted)
        return {u"It's corrupted, so its mods can't be changed. Run it as it is if you can take the "
                u"bad mods, or leave it."_s};

    QStringList out;
    if (bad == total) {
        out << u"Every mod on it is bad, so an Orb of Annulment takes off a bad one whichever it picks. "
               u"Or leave it."_s;
        return out;
    }
    out << u"An Orb of Annulment takes off one mod at random: a %1 chance it takes off a bad one, "
           u"otherwise it takes off a good one and the bad ones stay."_s.arg(oddsText(bad, total));
    // the omens that limit it to one side, when the copy said which is which
    int prefixes = 0;
    int suffixes = 0;
    int badPrefixes = 0;
    int badSuffixes = 0;
    bool sides = true;
    for (const MapMod &m : check.mods) {
        if (m.side == AffixSide::Unknown)
            sides = false;
        else if (m.side == AffixSide::Prefix) {
            ++prefixes;
            badPrefixes += m.isBad();
        } else {
            ++suffixes;
            badSuffixes += m.isBad();
        }
    }
    if (sides) {
        const bool prefixBetter = badPrefixes > 0 && badPrefixes * total > bad * prefixes;
        const bool suffixBetter = badSuffixes > 0 && badSuffixes * total > bad * suffixes;
        if (prefixBetter)
            out << u"With an Omen of Sinistral Annulment it only takes off a prefix: %1 chance it is a bad "
                   u"one."_s.arg(oddsText(badPrefixes, prefixes));
        if (suffixBetter)
            out << u"With an Omen of Dextral Annulment it only takes off a suffix: %1 chance it is a bad "
                   u"one."_s.arg(oddsText(badSuffixes, suffixes));
    }
    out << u"A Chaos Orb takes off a random mod and puts on a new random one: the same %1 chance to take "
           u"off a bad one, but the new mod can be bad too."_s.arg(oddsText(bad, total));
    const bool deadly =
        std::ranges::any_of(check.mods, [](const MapMod &m) { return m.danger == Danger::Deadly; });
    if (!deadly)
        out << u"Or run it as it is if you can take the risky mods."_s;
    else
        out << u"Don't run it as it is unless you are sure you can take the mods that can end the run."_s;
    return out;
}

} // namespace

MapCheck checkMap(const Item &item, MapKind kind, const MapBuild &build)
{
    MapCheck check;
    check.kind = kind;
    check.corrupted = item.corrupted;
    check.usedBuild = build.imported || build.resists || build.health > 0;
    for (const RawMod &raw : rawModsOf(item, kind))
        check.mods << rate(raw, build);
    check.buildChanged =
        std::ranges::any_of(check.mods, [](const MapMod &m) { return !m.buildNote.isEmpty(); });

    const int deadly =
        int(std::ranges::count_if(check.mods, [](const MapMod &m) { return m.danger == Danger::Deadly; }));
    const int risky = check.badMods() - deadly;
    QStringList names;
    for (const MapMod &m : check.mods) {
        if (m.isBad())
            names << nameOf(m);
    }
    if (check.mods.isEmpty()) {
        check.grade = MapGrade::Fine;
        check.summary = u"There are no mods on it to rate."_s;
    } else if (deadly > 0 || risky >= 2) {
        check.grade = MapGrade::Bad;
        check.summary = u"%1 of its mods are bad."_s.arg(check.badMods());
    } else if (risky == 1) {
        check.grade = MapGrade::Fine;
        check.summary = u"One mod to watch."_s;
    } else {
        check.grade = MapGrade::Good;
        check.summary = u"Nothing on it is dangerous."_s;
    }
    check.suggestions = check.mods.isEmpty() ? QStringList() : suggest(check);
    return check;
}

} // namespace pou
