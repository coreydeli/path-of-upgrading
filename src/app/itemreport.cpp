// SPDX-License-Identifier: MIT
#include "app/itemreport.h"

#include "core/numbers.h"
#include "core/resists.h"
#include "core/statnorm.h"
#include "core/wearing.h"

#include <QHash>
#include <QLocale>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

struct Words
{
    QString text;
    Tone::Tone tone = Tone::Plain;
};

Words wordsFor(Verdict verdict)
{
    switch (verdict) {
    case Verdict::New:
        return {u"Empty slot"_s, Tone::Plain};
    case Verdict::Same:
        return {u"Sidegrade"_s, Tone::Plain};
    case Verdict::Small:
        return {u"Small upgrade"_s, Tone::Good};
    case Verdict::Upgrade:
        return {u"Upgrade"_s, Tone::Good};
    case Verdict::Downgrade:
        return {u"Downgrade"_s, Tone::Bad};
    case Verdict::Risky:
        return {u"Risky upgrade"_s, Tone::Warning};
    case Verdict::ResistLoss:
        return {u"Loses resistance"_s, Tone::Bad};
    case Verdict::DamageLoss:
        return {u"Loses damage"_s, Tone::Bad};
    case Verdict::Equipped:
        return {u"Equipped"_s, Tone::Plain};
    }
    Q_UNREACHABLE_RETURN({});
}

// A change rounded to a decimal, with its sign: "+4", "-2.5".
QString signedNumber(double v)
{
    const double rounded = roundTo(v, 1);
    return (rounded > 0 ? u"+"_s : QString()) + formatNumber(rounded);
}

void putScore(ItemReport &report, const Evaluation &e)
{
    const Comparison &r = e.comparison;
    switch (e.mode) {
    case ScoreMode::PathOfBuilding:
        // a loss with a score above 0 would read like an upgrade
        if ((r.verdict == Verdict::ResistLoss || r.verdict == Verdict::DamageLoss) && r.delta > 0)
            return;
        report.score = signedNumber(r.delta) + u'%';
        report.scoreLabel = u"for your build"_s;
        return;
    case ScoreMode::GearScores:
        report.score = signedNumber(r.delta);
        report.scoreLabel = u"gear score"_s;
        return;
    case ScoreMode::StatWeights:
        report.score = signedNumber(r.delta);
        report.scoreLabel =
            r.percent ? u"by your weights (%1%)"_s.arg(signedNumber(*r.percent)) : u"by your weights"_s;
        return;
    }
}

void putPobStatus(ItemReport &report, const ItemCheck &check)
{
    switch (check.pob) {
    case ItemCheck::Pob::Checking:
        report.pobChecking = true;
        report.pobStatus = u"Checking with Path of Building"_s;
        return;
    case ItemCheck::Pob::Failed:
        report.pobStatus = check.pobError;
        return;
    case ItemCheck::Pob::NoBuild:
        report.pobStatus = u"Import this character's build from Path of Building for exact verdicts."_s;
        return;
    case ItemCheck::Pob::Unused:
    case ItemCheck::Pob::Checked:
        return;
    }
}

// "a", "a and b", "a, b and c"
QString listed(const QStringList &words)
{
    if (words.size() < 2)
        return words.value(0);
    return words.first(words.size() - 1).join(u", "_s) + u" and "_s + words.last();
}

QString capsOf(const PobVerdict &v)
{
    QStringList capped;
    for (const PobResistChange &r : v.resists) {
        if (r.reachesCap)
            capped << elementName(r.element).toLower();
    }
    if (capped.isEmpty())
        return {};
    return u"Caps your %1 %2"_s.arg(listed(capped), capped.size() == 1 ? u"resistance"_s : u"resistances"_s);
}

Tone::Tone toneOf(double change)
{
    if (change > 0)
        return Tone::Good;
    return change < 0 ? Tone::Bad : Tone::Plain;
}

QString damageLabel(const PobVerdict &v)
{
    return v.damage.stat == u"MinionDPS" ? u"Minion DPS"_s : u"DPS"_s;
}

QList<ReportFigure> headlineOf(const PobVerdict &v)
{
    QList<ReportFigure> headline;
    if (const std::optional<double> damage = v.damage.percent) {
        headline << ReportFigure{
            .label = damageLabel(v), .change = signedNumber(*damage) + u'%', .tone = toneOf(*damage)};
    }
    if (const std::optional<double> ehp = v.ehp ? v.ehp->percent : std::nullopt) {
        headline << ReportFigure{
            .label = u"Effective HP"_s, .change = signedNumber(*ehp) + u'%', .tone = toneOf(*ehp)};
    }
    if (v.resistGain != 0) {
        headline << ReportFigure{.label = u"Resistance toward the caps"_s,
                                 .change = signedNumber(v.resistGain),
                                 .tone = toneOf(v.resistGain)};
    }
    return headline;
}

// A stat as the popup shows it: whole from 100 up, with commas from 1,000.
QString amount(double v)
{
    if (std::abs(v) < 100)
        return formatNumber(roundTo(v, 1));
    static const QLocale english(QLocale::English);
    return english.toString(roundHalfUp(v), 'f', 0);
}

QString signedAmount(double v)
{
    return (v > 0 ? u"+"_s : QString()) + amount(v);
}

QString percent(double v)
{
    return formatNumber(roundHalfUp(v)) + u'%';
}

QString pobLabel(const QString &stat)
{
    static const QHash<QString, QString> labels = {
        {u"EnergyShield"_s, u"Energy Shield"_s},
        {u"SpiritUnreserved"_s, u"Unreserved Spirit"_s},
        {u"MinionLife"_s, u"Minion Life"_s},
    };
    return labels.value(stat, stat);
}

// Damage or effective HP before and after. It changes by percent, or by the
// number when it was 0 before.
ReportFigure bigFigure(const QString &label, const PobChange &c)
{
    const double change = c.percent.value_or(c.to - c.from);
    return {.label = label,
            .change = c.percent ? signedNumber(*c.percent) + u'%' : signedAmount(change),
            .from = amount(c.from),
            .to = amount(c.to),
            .tone = toneOf(change)};
}

bool isCalculated(const PobChange &c)
{
    return c.from > 0 || c.to > 0;
}

QList<ReportFigure> pobChanges(const PobVerdict &v)
{
    QList<ReportFigure> changes;
    if (isCalculated(v.damage))
        changes << bigFigure(damageLabel(v), v.damage);
    if (v.ehp && isCalculated(*v.ehp))
        changes << bigFigure(u"Effective HP"_s, *v.ehp);
    for (const PobResistChange &r : v.resists) {
        const double change = roundHalfUp(r.to) - roundHalfUp(r.from);
        // past the cap only the cap counts
        if (r.capped && r.wasCapped && change == 0)
            continue;
        ReportFigure f{.label = elementName(r.element) + u" res"_s,
                       .change = (change > 0 ? u"+"_s : QString()) + percent(change),
                       .from = percent(r.from),
                       .to = percent(r.to)};
        // a raised cap can leave you under it with what you had
        if (!r.capped)
            f.note = r.wasCapped && change < 0 ? u"off the cap"_s : u"under the cap"_s;
        else if (!r.wasCapped)
            f.note = u"capped"_s;
        f.tone = r.capped && !r.wasCapped ? Tone::Good : toneOf(change);
        changes << f;
    }
    for (const PobChange &c : v.changes) {
        changes << ReportFigure{.label = pobLabel(c.stat),
                                .change = signedAmount(c.to - c.from),
                                .from = amount(c.from),
                                .to = amount(c.to),
                                .tone = toneOf(c.to - c.from)};
    }
    return changes;
}

QList<ReportFigure> characterChanges(const CharacterChange &change)
{
    QList<ReportFigure> changes;
    for (const ResistChange &r : change.resists) {
        ReportFigure f{.label = r.label, .change = signedNumber(r.delta) + u'%', .tone = toneOf(r.delta)};
        if (r.from && r.to) {
            f.from = formatNumber(roundTo(*r.from, 1)) + u'%';
            f.to = formatNumber(roundTo(*r.to, 1)) + u'%';
            const bool thinner = r.thin && r.delta < 0;
            QStringList notes;
            if (r.under)
                notes << u"under the cap"_s;
            if (thinner)
                notes << u"barely capped"_s;
            if (const double over = roundTo(*r.to - r.cap, 1); over > 0)
                notes << formatNumber(over) + u" over the cap"_s;
            f.note = notes.join(u", "_s);
            if (r.delta > 0)
                f.tone = r.wasUnder ? Tone::Good : Tone::Plain;
            else
                f.tone = r.under || thinner ? Tone::Bad : Tone::Plain;
            // over the cap either way
            f.minor = !r.under && !r.wasUnder && !thinner;
        }
        changes << f;
    }
    for (const TotalChange &t : change.totals) {
        changes << ReportFigure{.label = t.label,
                                .change = signedAmount(t.delta),
                                .from = amount(t.from),
                                .to = amount(t.to),
                                .tone = toneOf(t.delta)};
    }
    if (const std::optional<TotalChange> &overall = change.overall; overall && overall->delta != 0) {
        changes << ReportFigure{.label = u"Average gear score"_s,
                                .change = signedAmount(overall->delta),
                                .from = amount(overall->from),
                                .to = amount(overall->to),
                                .tone = toneOf(overall->delta)};
    }
    return changes;
}

// A stat's line with the change in it, "+30 to maximum Life".
QString statLine(const QString &key, double change)
{
    const QString number = signedAmount(change);
    // a range counts as its average
    if (const qsizetype at = key.indexOf(u"# to #"); at >= 0)
        return QString(key).replace(at, 6, number + u" average"_s);
    if (const qsizetype at = key.indexOf(u'#'); at >= 0)
        return QString(key).replace(at, 1, number);
    // the item's own, "[Base] Armour"
    return number + u' ' + statName(key);
}

QList<ReportFigure> statsOf(const QList<StatRow> &rows)
{
    QList<ReportFigure> stats;
    for (const StatRow &row : rows) {
        if (row.usefulness == 0)
            continue;
        stats << ReportFigure{.label = statLine(row.key, row.diff),
                              .tone = toneOf(row.diff * row.usefulness)};
        if (stats.size() == 4)
            break;
    }
    return stats;
}

QString openAffixesOf(const Affixes &affixes)
{
    QStringList open;
    if (affixes.openPrefix > 0)
        open << (affixes.openPrefix == 1 ? u"1 prefix"_s : u"%1 prefixes"_s.arg(affixes.openPrefix));
    if (affixes.openSuffix > 0)
        open << (affixes.openSuffix == 1 ? u"1 suffix"_s : u"%1 suffixes"_s.arg(affixes.openSuffix));
    if (open.isEmpty())
        return u"No open affixes"_s;
    return u"Open affixes: "_s + open.join(u", "_s);
}

bool isUnique(const Item &item)
{
    return item.rarity.compare(u"Unique", Qt::CaseInsensitive) == 0;
}

} // namespace

ItemReport reportOf(const ItemCheck &check)
{
    const Item &item = check.item;
    const Evaluation &e = check.evaluation;
    const Comparison &r = e.comparison;

    ItemReport report;
    report.name = item.name;
    report.rarity = item.rarity;
    report.gear = !item.slot.isEmpty();
    if (!r.isEquipped)
        report.wearIn = slotsFor(item);
    if (!item.baseType.isEmpty() && item.baseType != item.name)
        report.details << item.baseType;
    if (!item.itemClass.isEmpty())
        report.details << item.itemClass;
    if (item.itemLevel > 0)
        report.details << u"Item level %1"_s.arg(item.itemLevel);
    if (item.affixes.known)
        report.openAffixes = openAffixesOf(item.affixes);
    if (!r.ok) {
        report.notes << r.reason;
        return report;
    }

    // a unique's best mods are often ones no weight measures, so its verdict
    // is Path of Building's or none
    if (isUnique(item) && e.mode != ScoreMode::PathOfBuilding && !r.isEquipped) {
        report.verdict = u"Unique"_s;
        if (check.pob != ItemCheck::Pob::Checking)
            report.notes << u"Uniques have fixed mods, so they don't get a gear score."_s;
    } else {
        const Words verdict = wordsFor(r.verdict);
        report.verdict = verdict.text;
        report.verdictTone = verdict.tone;
        // in an empty slot the score is just the item's own, but Path of
        // Building still says how much better the build gets
        if (!r.isEquipped && (!r.replacing.isEmpty() || e.mode == ScoreMode::PathOfBuilding))
            putScore(report, e);
    }

    if (r.isEquipped)
        report.versus = u"in your %1 slot"_s.arg(r.targetSlot);
    else if (!r.replacing.isEmpty())
        report.versus = u"vs "_s + r.replacing.join(u" and "_s);
    report.losses = e.losses;
    for (const QString &risk : e.risks) {
        if (!e.losses.contains(risk))
            report.risks << risk;
    }
    if (e.pob) {
        report.caps = capsOf(e.pob->verdict);
        report.headline = headlineOf(e.pob->verdict);
        report.changes = pobChanges(e.pob->verdict);
    } else if (e.change) {
        report.changes = characterChanges(*e.change);
    }
    report.stats = statsOf(r.rows);
    if (e.gear && e.gear->isRated() && e.mode != ScoreMode::PathOfBuilding) {
        report.gearScore = QString::number(e.gear->score);
        report.gearGrade = e.gear->grade;
        report.gearBuild = e.gear->buildName;
        if (e.gearEquipped && e.gearEquipped->isRated())
            report.yourGearScore = QString::number(e.gearEquipped->score);
    }
    putPobStatus(report, check);
    report.notes << r.notes;
    return report;
}

} // namespace pou
