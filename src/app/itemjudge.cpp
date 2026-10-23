// SPDX-License-Identifier: MIT
#include "app/itemjudge.h"

#include "core/pobeval.h"
#include "services/pobengine.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

ItemJudge::ItemJudge(QObject *parent)
    : QObject(parent)
{}

ItemJudge::~ItemJudge()
{
    delete m_reply;
}

void ItemJudge::setCharacter(const Character &character)
{
    m_character = character;
}

void ItemJudge::setBuild(const std::optional<PobBuild> &build)
{
    m_build = build;
}

void ItemJudge::setEngine(PobEngine *engine)
{
    m_engine = engine;
}

void ItemJudge::setPriorities(const PobPriorities &priorities)
{
    m_options.priorities = priorities;
}

void ItemJudge::setDamageWeight(double weight)
{
    m_options.damageWeight = weight;
}

void ItemJudge::check(const Item &item)
{
    // the answer for the item before isn't wanted any more
    delete m_reply;
    m_check = {.item = item, .evaluation = evaluate(item, m_character, m_options.priorities)};
    const Comparison &r = m_check.evaluation.comparison;
    if (r.ok && !r.isEquipped && m_engine && m_engine->state() != PobEngine::State::Unavailable) {
        if (m_build) {
            m_check.pob = ItemCheck::Pob::Checking;
            // tried in each place it can go in
            m_reply = m_engine->evaluate({{.text = item.raw}});
            connect(m_reply, &PobReply::finished, this, &ItemJudge::answered);
        } else {
            m_check.pob = ItemCheck::Pob::NoBuild;
        }
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
    const PobItemEval tried = eval.items.value(0);
    PobVerdictOptions options = m_options;
    options.ignoreChaos = m_character.chaosInoculation;
    const std::optional<PobJudgement> best = bestPlacement(eval.before, tried, m_check.item, options);
    if (!best) {
        fail(tried.error.isEmpty() ? u"Path of Building didn't say how the item does."_s : tried.error);
        return;
    }
    // compared with what you wear where it does best, which isn't always
    // where the gear score would put it
    const QString slot = gearSlot(best->placement.slot, m_build && m_build->swapInUse);
    m_check.evaluation = withPob(evaluate(m_check.item, m_character, m_options.priorities, slot), *best);
    m_check.pob = ItemCheck::Pob::Checked;
    Q_EMIT checked(m_check);
}

void ItemJudge::fail(const QString &why)
{
    m_check.pob = ItemCheck::Pob::Failed;
    m_check.pobError = why;
    Q_EMIT checked(m_check);
}

} // namespace pou
