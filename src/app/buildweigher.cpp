// SPDX-License-Identifier: MIT
#include "app/buildweigher.h"

#include "core/pobbuild.h"
#include "core/pobweighing.h"
#include "services/logging.h"
#include "services/pobengine.h"

#include <QDate>
#include <QLoggingCategory>
#include <memory>

using namespace Qt::Literals::StringLiterals;

namespace pou {

namespace {

// The slot Path of Building is asked about for the group, or empty for a group
// it has no slot for. Rings are the same either way.
QString slotOf(const QString &group)
{
    if (group == u"Jewel")
        return {};
    return group == u"Ring" ? u"Ring 1"_s : group;
}

constexpr int retryMs = 500;

} // namespace

BuildWeigher::BuildWeigher(ActiveCharacter *character, PobEngine *pob, QObject *parent)
    : QObject(parent)
    , m_character(character)
    , m_pob(pob)
{
    m_retry.setSingleShot(true);
    m_retry.setInterval(retryMs);
    connect(&m_retry, &QTimer::timeout, this, &BuildWeigher::next);
    // it has the build's numbers once it has loaded it, which is when to start
    connect(character, &ActiveCharacter::changed, this, &BuildWeigher::consider);
    connect(pob, &PobEngine::installChanged, this, &BuildWeigher::consider);
    connect(character, &ActiveCharacter::pobStatsChanged, this, &BuildWeigher::consider);
}

void BuildWeigher::setSettings(const PobSettings &settings)
{
    // what it has weighed by the old balance doesn't go with the rest
    if (m_running && settings.damageWeight != m_settings.damageWeight)
        cancel();
    m_settings = settings;
    consider();
}

bool BuildWeigher::canWeigh() const
{
    if (!m_settings.enabled || !m_character->exists() || !m_character->build())
        return false;
    // judged by your own weights, or by a profile that isn't there
    const Character &character = m_character->character();
    return !character.weightsOnly && buildProfile(character.buildProfile);
}

bool BuildWeigher::weighAgain()
{
    if (!canWeigh())
        return false;
    m_gaveUp.clear();
    cancel();
    begin();
    return true;
}

bool BuildWeigher::upToDate(const WeighedProfile &weighed) const
{
    const QString version = m_pob->version();
    return weighed.build == buildFingerprint(m_character->pobXml()) &&
           weighed.damageWeight == m_settings.damageWeight &&
           (version.isEmpty() || weighed.pobVersion == version);
}

QString BuildWeigher::attempt() const
{
    return buildFingerprint(m_character->pobXml()) + u'/' + QString::number(m_settings.damageWeight) + u'/' +
           m_pob->version();
}

bool BuildWeigher::needed() const
{
    if (!canWeigh())
        return false;
    const Character &character = m_character->character();
    if (!character.weighed)
        return m_gaveUp != attempt();
    return !upToDate(*character.weighed) && m_gaveUp != attempt();
}

void BuildWeigher::consider()
{
    if (m_running) {
        // for another character or another build, or with Path of Building
        // off, what it's doing is no use
        if (!canWeigh() || m_character->id() != m_for || buildFingerprint(m_character->pobXml()) != m_build) {
            cancel();
        } else {
            return;
        }
    }
    if (!needed())
        return;
    if (!m_character->pobStats()) {
        // which starts it, as it only does when it's asked something
        m_pob->load();
        return;
    }
    begin();
}

void BuildWeigher::begin()
{
    if (!m_character->pobStats()) {
        m_pob->load();
        return;
    }
    m_for = m_character->id();
    m_build = buildFingerprint(m_character->pobXml());
    m_left = ratedGroups();
    m_done.clear();
    ++m_run;
    setRunning(true);
    next();
}

void BuildWeigher::next()
{
    if (!m_running)
        return;
    if (m_busy && m_busy()) {
        m_retry.start();
        return;
    }
    const std::optional<PobBuild> &build = m_character->build();
    const std::optional<PobStats> &stats = m_character->pobStats();
    while (!m_left.isEmpty()) {
        const QString group = m_left.takeFirst();
        const QString slot = build ? pobSlot(slotOf(group), build->swapInUse) : QString();
        if (slot.isEmpty() || !stats)
            continue;
        m_reply = m_pob->weigh(slot, pobSearchStats(*stats, m_settings.damageWeight));
        const int run = m_run;
        connect(m_reply, &PobReply::finished, this, [this, run, group] {
            if (run == m_run)
                answered(group);
        });
        return;
    }
    finish();
}

void BuildWeigher::answered(const QString &group)
{
    const std::unique_ptr<PobReply> reply(m_reply.data());
    m_reply.clear();
    if (reply->errorString().isEmpty()) {
        const SlotWeighing weighing = slotWeighing(group, readPobWeighing(reply->answer()));
        if (!weighing.mods.isEmpty())
            m_done << weighing;
    } else {
        // and that group goes by the profile it started from
        qCWarning(lcPob, "Path of Building couldn't weigh the mods for %s: %s", qUtf8Printable(group),
                  qUtf8Printable(reply->errorString()));
    }
    next();
}

void BuildWeigher::finish()
{
    const QList<SlotWeighing> done = std::move(m_done);
    m_done.clear();
    const QString attempted = attempt();
    setRunning(false);
    const Character &character = m_character->character();
    const BuildProfile *start = buildProfile(character.buildProfile);
    if (!start || m_character->id() != m_for || !m_character->exists())
        return;
    WeighedProfile weighed = weighedProfile(done, *start);
    if (weighed.slots.isEmpty()) {
        // keeps what it had, and doesn't ask again until something changes
        m_gaveUp = attempted;
        return;
    }
    weighed.pobVersion = m_pob->version();
    weighed.date = QDate::currentDate().toString(Qt::ISODate);
    weighed.damageWeight = m_settings.damageWeight;
    weighed.build = m_build;
    m_gaveUp.clear();
    m_character->change([&weighed](Character &c) { c.weighed = weighed; });
}

void BuildWeigher::cancel()
{
    m_retry.stop();
    ++m_run;
    // which calls off what it asked
    delete m_reply;
    m_left.clear();
    m_done.clear();
    setRunning(false);
}

void BuildWeigher::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    Q_EMIT weighingChanged();
}

} // namespace pou
