// SPDX-License-Identifier: MIT
#include "app/itemreport.h"

#include "core/numbers.h"

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
    if (!item.baseType.isEmpty() && item.baseType != item.name)
        report.details << item.baseType;
    if (!item.itemClass.isEmpty())
        report.details << item.itemClass;
    if (item.itemLevel > 0)
        report.details << u"Item level %1"_s.arg(item.itemLevel);
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
    putPobStatus(report, check);
    report.notes << r.notes;
    return report;
}

} // namespace pou
