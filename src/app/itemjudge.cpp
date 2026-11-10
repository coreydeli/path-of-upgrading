// SPDX-License-Identifier: MIT
#include "app/itemjudge.h"

#include "core/pobeval.h"
#include "services/pobengine.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

MapBuild mapBuildOf(const Character &character, const std::optional<PobBuild> &build)
{
    MapBuild out;
    out.imported = build.has_value();
    if (character.resists)
        out.resists = liveResists(*character.resists, character.gear);
    if (build)
        out.health = build->life + build->energyShield;
    return out;
}

ItemCheck firstCheck(const Item &item, const Judging &judging)
{
    if (judging.maps) {
        if (const std::optional<MapKind> kind = mapKindOf(item)) {
            ItemCheck check{.item = item};
            check.map = checkMap(item, *kind, mapBuildOf(judging.character, judging.build));
            return check;
        }
    }
    ItemCheck check{.item = item,
                    .evaluation = evaluate(item, judging.character, judging.options.priorities)};
    const Comparison &r = check.evaluation.comparison;
    if (r.ok && !r.isEquipped && judging.engine && judging.engine->state() != PobEngine::State::Unavailable)
        check.pob = judging.build ? ItemCheck::Pob::Checking : ItemCheck::Pob::NoBuild;
    return check;
}

namespace {

PobVerdictOptions pobOptions(const Judging &judging)
{
    PobVerdictOptions options = judging.options;
    options.ignoreChaos = judging.character.chaosInoculation;
    return options;
}

} // namespace

ItemCheck withPobAnswer(ItemCheck check, const PobStats &before, const PobItemEval &tried,
                        const Judging &judging)
{
    const std::optional<PobJudgement> best = bestPlacement(before, tried, check.item, pobOptions(judging));
    if (!best) {
        check.pob = ItemCheck::Pob::Failed;
        check.pobError =
            tried.error.isEmpty() ? u"Path of Building didn't say how the item does."_s : tried.error;
        return check;
    }
    // compared with what you wear where it does best, which isn't always
    // where the gear score would put it
    const QString slot = gearSlot(best->placement.slot, judging.build && judging.build->swapInUse);
    check.evaluation =
        withPob(evaluate(check.item, judging.character, judging.options.priorities, slot), *best);
    check.pob = ItemCheck::Pob::Checked;
    return check;
}

ItemJudge::ItemJudge(QObject *parent)
    : QObject(parent)
{}

ItemJudge::~ItemJudge()
{
    delete m_reply;
}

void ItemJudge::setCharacter(const Character &character)
{
    m_judging.character = character;
}

void ItemJudge::setBuild(const std::optional<PobBuild> &build)
{
    m_judging.build = build;
}

void ItemJudge::setEngine(PobEngine *engine)
{
    m_judging.engine = engine;
}

void ItemJudge::setPriorities(const PobPriorities &priorities)
{
    m_judging.options.priorities = priorities;
}

void ItemJudge::setDamageWeight(double weight)
{
    m_judging.options.damageWeight = weight;
}

void ItemJudge::setMaps(bool on)
{
    m_judging.maps = on;
}

void ItemJudge::check(const Item &item)
{
    // the answer for the item before isn't wanted any more
    delete m_reply;
    m_check = firstCheck(item, m_judging);
    const Comparison &r = m_check.evaluation.comparison;
    if (r.ok && !r.isEquipped)
        m_check.places = gearPlaces(item, m_judging.character, m_judging.options.priorities, r.targetSlot);
    if (m_check.pob == ItemCheck::Pob::Checking) {
        // tried in each place it can go in
        m_reply = m_judging.engine->evaluate({{.text = item.raw}});
        connect(m_reply, &PobReply::finished, this, &ItemJudge::answered);
    }
    Q_EMIT checked(m_check);
}

void ItemJudge::answered()
{
    PobReply *reply = m_reply;
    reply->deleteLater();
    if (!reply->errorString().isEmpty()) {
        fail(reply->errorString());
        return;
    }

    const PobEval eval = readPobEval(reply->answer());
    const PobItemEval &tried = eval.items.value(0);
    m_check = withPobAnswer(m_check, eval.before, tried, m_judging);
    // it tried the item in every place already, so these cost it nothing
    if (m_check.pob == ItemCheck::Pob::Checked)
        m_check.places = pobPlaces(judgePlacements(eval.before, tried, m_check.item, pobOptions(m_judging)));
    Q_EMIT checked(m_check);
}

void ItemJudge::fail(const QString &why)
{
    m_check.pob = ItemCheck::Pob::Failed;
    m_check.pobError = why;
    Q_EMIT checked(m_check);
}

} // namespace pou
