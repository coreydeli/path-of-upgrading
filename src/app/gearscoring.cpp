// SPDX-License-Identifier: MIT
#include "app/gearscoring.h"

#include "app/activecharacter.h"
#include "app/buildweigher.h"
#include "core/weighedprofile.h"

#include <QDate>
#include <QLocale>
#include <algorithm>
#include <array>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// What each of the levels multiplies a stat's importance by.
constexpr std::array levelValues = {0.0, 0.5, 1.0, 1.5, 2.0, 3.0};

// What each level is called, beside its factor in levelValues.
constexpr std::array<QStringView, 6> levelNames = {u"Not at all",     u"Half",   u"Normal",
                                                   u"One and a half", u"Double", u"Three times"};
static_assert(levelNames.size() == levelValues.size());

QString listed(const QStringList &items)
{
    return QLocale(QLocale::English).createSeparatedList(items);
}

// "Counts on Boots", or "Counts in every slot but Weapon and Jewel".
QString whereItCounts(const QStringList &groups)
{
    const QStringList &all = ratedGroups();
    if (groups.size() == all.size())
        return u"Counts in every slot"_s;
    if (groups.size() * 2 <= all.size())
        return u"Counts on %1"_s.arg(listed(groups));
    QStringList others;
    for (const QString &group : all) {
        if (!groups.contains(group))
            others << group;
    }
    return u"Counts in every slot but %1"_s.arg(listed(others));
}

// The default one for none.
QString idOf(const Character &character)
{
    return character.buildProfile.isEmpty() ? defaultBuildProfile.toString() : character.buildProfile;
}

} // namespace

GearScoring::GearScoring(ActiveCharacter *active, BuildWeigher *weigher, QObject *parent)
    : QAbstractListModel(parent)
    , m_active(active)
    , m_weigher(weigher)
{
    connect(active, &ActiveCharacter::changed, this, &GearScoring::refresh);
    connect(weigher, &BuildWeigher::weighingChanged, this, &GearScoring::changed);
    refresh();
}

int GearScoring::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_stats.size());
}

QVariant GearScoring::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const TweakableStat &stat = m_stats.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return stat.label;
    case WhereRole:
        if (untweaked(stat.label) == 0)
            return QString(whereItCounts(stat.groups) +
                           u". Not at all to start with, as your build has Chaos Inoculation"_s);
        return whereItCounts(stat.groups);
    case LevelRole:
        return levelOf(stat.label);
    }
    return {};
}

QHash<int, QByteArray> GearScoring::roleNames() const
{
    return {{LabelRole, "label"}, {WhereRole, "where"}, {LevelRole, "level"}};
}

QString GearScoring::character() const
{
    return m_active->character().name;
}

QString GearScoring::state() const
{
    const Character &character = m_active->character();
    if (character.weightsOnly)
        return u"Judged by your stat weights alone"_s;
    if (m_weigher->isWeighing())
        return u"Weighing your build with Path of Building"_s;
    if (character.weighed)
        return u"Weighed by Path of Building for your build on %1."_s.arg(weighedOn());
    return u"A general start, until Path of Building weighs your build."_s;
}

QString GearScoring::summary() const
{
    const Character &character = m_active->character();
    if (character.weightsOnly)
        return u"Items are judged by your stat weights alone, with no gear score."_s;
    const BuildProfile *p = buildProfile(character.buildProfile);
    if (!p)
        return {};
    if (character.weighed)
        return u"Slots Path of Building didn't weigh go by the %1 profile: %2"_s.arg(p->name, p->summary);
    return p->summary;
}

QString GearScoring::weighedOn() const
{
    const std::optional<WeighedProfile> &weighed = m_active->character().weighed;
    if (!weighed)
        return {};
    const QDate date = QDate::fromString(weighed->date, Qt::ISODate);
    return date.isValid() ? QLocale().toString(date, QLocale::LongFormat) : weighed->date;
}

bool GearScoring::isWeighing() const
{
    return m_weigher->isWeighing();
}

bool GearScoring::canWeigh() const
{
    return m_weigher->canWeigh();
}

bool GearScoring::weightsOnly() const
{
    return m_active->character().weightsOnly;
}

void GearScoring::setWeightsOnly(bool on)
{
    if (on == weightsOnly())
        return;
    change([on](Character &character) { character.weightsOnly = on; });
}

void GearScoring::weighAgain()
{
    m_weigher->weighAgain();
}

QStringList GearScoring::levels() const
{
    // "Half (0.5x)", the saved tweaks are the factors, not these
    QStringList labels;
    for (size_t i = 0; i < levelNames.size(); ++i)
        labels << levelNames[i] % u" ("_s % QString::number(levelValues[i]) % u"x)"_s;
    return labels;
}

bool GearScoring::isTweaked() const
{
    const ProfileTweaks &tweaks = m_active->character().tweaks;
    return std::ranges::any_of(m_stats,
                               [&tweaks](const TweakableStat &s) { return tweaks.contains(s.label); });
}

void GearScoring::setLevel(const QString &label, int level)
{
    if (level < 0 || level >= int(levelValues.size()) || level == levelOf(label))
        return;
    const double value = levelValues.at(level);
    const bool usual = value == untweaked(label);
    change([&](Character &character) {
        if (usual)
            character.tweaks.remove(label);
        else
            character.tweaks.insert(label, value);
    });
}

void GearScoring::reset()
{
    if (!isTweaked())
        return;
    change([this](Character &character) {
        for (const TweakableStat &s : std::as_const(m_stats))
            character.tweaks.remove(s.label);
    });
}

void GearScoring::refresh()
{
    const Character &character = m_active->character();
    QList<TweakableStat> stats;
    // none with your weights alone, as the profile isn't used
    if (const BuildProfile *start = character.weightsOnly ? nullptr : buildProfile(idOf(character))) {
        stats = tweakableStats(ratedProfile(*start, character.weighed));
        // a stat the start profile counts only where the weighing took over
        if (character.weighed)
            stats.removeIf([](const TweakableStat &s) { return s.groups.isEmpty(); });
    }
    const bool same = std::ranges::equal(stats, m_stats, [](const TweakableStat &a, const TweakableStat &b) {
        return a.label == b.label && a.groups == b.groups;
    });
    if (same) {
        if (!m_stats.isEmpty())
            Q_EMIT dataChanged(index(0), index(int(m_stats.size()) - 1), {WhereRole, LevelRole});
    } else {
        beginResetModel();
        m_stats = stats;
        endResetModel();
    }
    Q_EMIT changed();
}

void GearScoring::change(const std::function<void(Character &)> &edit)
{
    setError(m_active->change(edit) ? QString() : m_active->errorString());
}

// What it counts for without a tweak of yours.
double GearScoring::untweaked(const QString &label) const
{
    return m_active->character().chaosInoculation && label == resistLabel(Element::Chaos) ? 0 : 1;
}

int GearScoring::levelOf(const QString &label) const
{
    const double value = m_active->character().tweaks.value(label, untweaked(label));
    // the nearest, for one set by hand in the file
    const auto nearest =
        std::ranges::min_element(levelValues, {}, [value](double v) { return std::abs(v - value); });
    return int(nearest - levelValues.begin());
}

void GearScoring::setError(const QString &why)
{
    if (why == m_error)
        return;
    m_error = why;
    Q_EMIT errorChanged();
}

} // namespace pou
