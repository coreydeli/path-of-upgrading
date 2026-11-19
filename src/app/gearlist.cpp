// SPDX-License-Identifier: MIT
#include "app/gearlist.h"

#include "app/activecharacter.h"
#include "app/notehelp.h"
#include "app/settingsstore.h"
#include "core/character.h"
#include "core/evaluation.h"
#include "core/hands.h"
#include "core/slots.h"
#include "core/wornmods.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

GearNote noteOf(const QString &label, const QStringList &stats, const QString &between = u", "_s)
{
    return {.label = label, .text = stats.join(between), .stats = stats, .help = noteHelp(label)};
}

// The ways a gear score falls short, a note each.
QList<GearNote> notesOf(const GearScore &gear)
{
    QList<GearNote> notes;
    if (!gear.missing.isEmpty())
        notes << noteOf(u"Missing stats"_s, missingStats(gear));
    if (!gear.notScored.isEmpty())
        notes << noteOf(u"Not counted"_s, notScoredStats(gear));
    if (const QStringList weak = weakRolls(gear); !weak.isEmpty())
        notes << noteOf(u"Weak rolls"_s, weak);
    if (const QStringList next = nextTiers(gear); !next.isEmpty())
        notes << noteOf(u"Next tier"_s, next, u"; "_s);
    return notes;
}

// A unique has no gear score, so say what judges it instead. These are plain
// sentences, so no stats, and the first matches the popup's.
QList<GearNote> uniqueNotes()
{
    return {
        GearNote{.label = u"Unique"_s,
                 .text = u"Uniques have fixed mods, so they don't get a gear score."_s,
                 .stats = {}},
        GearNote{.label = u"Judged by"_s,
                 .text = u"What it does for your damage and defences in Path of Building, once a build is "
                         u"loaded. Without that it gets no verdict."_s,
                 .stats = {}},
    };
}

// What the build wants most in a slot with nothing in it.
QList<GearNote> wantedIn(const QString &slot, const GearScoreOptions &options)
{
    const SlotMods &mods = options.mods ? *options.mods : SlotMods::builtIn();
    const QString itemClass = classOfSlot(slot);
    QStringList labels;
    for (const KeyStat &stat : topStats(slot, 3, options)) {
        const auto best = mods.best(itemClass, stat.key);
        labels << (best ? stat.label + u" up to "_s + rollText(best->high) : stat.label);
    }
    if (labels.isEmpty())
        return {};
    return {noteOf(u"Your build wants"_s, labels)};
}

QList<ModLine> modLinesOf(const Item &item)
{
    QList<ModLine> lines;
    for (const WornMod &mod : wornMods(item)) {
        lines << ModLine{.group = mod.group,
                         .text = mod.text,
                         .fractured = mod.fractured,
                         .starts = lines.isEmpty() || lines.last().group != mod.group};
    }
    return lines;
}

} // namespace

GearList::GearList(ActiveCharacter *active, SettingsStore *settings, QObject *parent)
    : QAbstractListModel(parent)
    , m_active(active)
    , m_settings(settings)
    , m_priorities(settings->settings().pob.priorities)
{
    connect(active, &ActiveCharacter::changed, this, &GearList::refresh);
    // which rate resistances higher when they come first
    connect(settings, &SettingsStore::changed, this, [this] {
        if (m_settings->settings().pob.priorities != m_priorities)
            refresh();
    });
    refresh();
}

int GearList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant GearList::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Row &row = m_rows.at(index.row());
    const Character &character = m_active->character();
    const auto item = character.gear.constFind(row.slot);
    const bool worn = item != character.gear.cend();
    switch (role) {
    case Qt::DisplayRole:
    case SlotRole:
        return row.slot;
    case NameRole:
        return worn ? item->name : QString();
    case BaseRole:
        return worn && item->baseType != item->name ? item->baseType : QString();
    case FromPobRole:
        return character.fromPob.contains(row.slot);
    case ScoreRole:
        return row.score ? QString::number(*row.score) : QString();
    case GradeRole:
        return row.grade;
    case ExtraRole:
        return !mainSlots().contains(row.slot);
    case NotesRole:
        return QVariant::fromValue(row.notes);
    case RarityRole:
        return worn ? item->rarity : QString();
    case ItemLevelRole:
        return worn ? item->itemLevel : 0;
    case ModsRole:
        return worn ? QVariant::fromValue(modLinesOf(*item)) : QVariant::fromValue(QList<ModLine>());
    case QualityRole:
        return worn ? item->quality : 0;
    case CorruptedRole:
        return worn && item->corrupted;
    case FracturedRole:
        return worn && hasFracturedMod(*item);
    case FirstRole:
        return index.row() == 0 ||
               mainSlots().contains(m_rows.at(index.row() - 1).slot) != mainSlots().contains(row.slot);
    }
    return {};
}

QHash<int, QByteArray> GearList::roleNames() const
{
    return {
        {SlotRole, "slot"},       {NameRole, "name"},           {BaseRole, "base"},
        {FromPobRole, "fromPob"}, {ScoreRole, "score"},         {GradeRole, "grade"},
        {ExtraRole, "extra"},     {NotesRole, "notes"},         {FirstRole, "first"},
        {RarityRole, "rarity"},   {ItemLevelRole, "itemLevel"}, {ModsRole, "mods"},
        {QualityRole, "quality"}, {CorruptedRole, "corrupted"}, {FracturedRole, "fractured"},
    };
}

QString GearList::character() const
{
    return m_active->character().name;
}

bool GearList::hasExtras() const
{
    return std::ranges::any_of(m_rows, [](const Row &row) { return !mainSlots().contains(row.slot); });
}

bool GearList::takeOff(const QString &slot)
{
    if (!m_active->takeOff(slot)) {
        setError(m_active->errorString());
        return false;
    }
    setError({});
    return true;
}

void GearList::refresh()
{
    m_priorities = m_settings->settings().pob.priorities;
    const Character &character = m_active->character();

    beginResetModel();
    m_rows.clear();
    // the main slots are there to fill even before there's a character
    for (const QString &slot : allSlots()) {
        const auto item = character.gear.constFind(slot);
        const GearScoreOptions options = gearScoreOptions(character, slot, m_priorities);
        if (item == character.gear.cend()) {
            // a two-handed weapon leaves the offhand out of use
            if (mainSlots().contains(slot) && !offhandOutOfUse(character.gear, character.wielding, slot))
                m_rows << Row{
                    .slot = slot, .score = std::nullopt, .grade = {}, .notes = wantedIn(slot, options)};
            continue;
        }
        Row row{.slot = slot};
        const std::optional<GearScore> score = gearScore(*item, options);
        if (score && score->isRated()) {
            row.score = score->score;
            row.grade = score->grade;
            row.notes = notesOf(*score);
        } else if (score && score->unrated == GearScore::Unrated::Unique) {
            row.notes = uniqueNotes();
        }
        m_rows << row;
    }

    // the scores worked out above
    const GearScoreFn slotScore = [this](const Item &, const QString &slot) -> std::optional<double> {
        for (const Row &row : std::as_const(m_rows)) {
            if (row.slot == slot)
                return row.score;
        }
        return std::nullopt;
    };
    const std::optional<double> overall = overallGearScore(character.gear, slotScore);
    m_overall = overall ? QString::number(*overall) : QString();
    m_overallGrade = overall ? gearGrade(int(*overall)) : QString();
    endResetModel();
    Q_EMIT changed();
}

void GearList::setError(const QString &why)
{
    if (why == m_error)
        return;
    m_error = why;
    Q_EMIT errorChanged();
}

} // namespace pou
