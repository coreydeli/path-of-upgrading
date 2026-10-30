// SPDX-License-Identifier: MIT
#include "app/gearscoring.h"

#include "app/activecharacter.h"

#include <QLocale>
#include <algorithm>
#include <array>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// What each of the levels multiplies a stat's importance by.
constexpr std::array levelValues = {0.0, 0.5, 1.0, 1.5, 2.0, 3.0};

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

GearScoring::GearScoring(ActiveCharacter *active, QObject *parent)
    : QAbstractListModel(parent)
    , m_active(active)
{
    connect(active, &ActiveCharacter::changed, this, &GearScoring::refresh);
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

QStringList GearScoring::profiles() const
{
    QStringList names;
    for (const BuildProfile &p : buildProfiles())
        names << p.name;
    return names << u"None, by my stat weights alone"_s;
}

int GearScoring::profile() const
{
    // none, after the profiles, when no profile has the id
    const QList<BuildProfile> &all = buildProfiles();
    return int(std::ranges::find(all, idOf(m_active->character()), &BuildProfile::id) - all.begin());
}

void GearScoring::setProfile(int profile)
{
    const QList<BuildProfile> &all = buildProfiles();
    if (profile < 0 || profile > all.size())
        return;
    // choosing the one you have is a choice too, but only the first time
    if (profile == this->profile() && m_active->character().profilePicked)
        return;
    const QString id = profile < all.size() ? all.at(profile).id : noBuildProfile.toString();
    change([&id](Character &character) {
        character.buildProfile = id;
        character.profilePicked = true;
    });
}

QString GearScoring::summary() const
{
    const BuildProfile *p = buildProfile(m_active->character().buildProfile);
    return p ? p->summary : u"Items are judged by your stat weights alone, with no gear score."_s;
}

QStringList GearScoring::levels() const
{
    return {u"Not at all"_s,    u"Half as much"_s,       u"As usual"_s, u"One and a half times as much"_s,
            u"Twice as much"_s, u"Three times as much"_s};
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
    const QString id = m_active->exists() ? idOf(m_active->character()) : QString();
    if (id == m_profile) {
        if (!m_stats.isEmpty())
            Q_EMIT dataChanged(index(0), index(int(m_stats.size()) - 1), {WhereRole, LevelRole});
    } else {
        beginResetModel();
        m_profile = id;
        const BuildProfile *p = id.isEmpty() ? nullptr : buildProfile(id);
        m_stats = p ? tweakableStats(*p) : QList<TweakableStat>();
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
