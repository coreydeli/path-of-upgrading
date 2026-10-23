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
    m_priorities = priorities;
}

void ItemJudge::check(const Item &item)
{
    // the answer for the item before isn't wanted any more
    delete m_reply;
    m_check = {.item = item, .evaluation = evaluate(item, m_character, m_priorities)};
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
    const std::optional<PobJudgement> best =
        bestPlacement(eval.before, tried, m_check.item, {.priorities = m_priorities});
    if (!best) {
        fail(tried.error.isEmpty() ? u"Path of Building didn't say how the item does."_s : tried.error);
        return;
    }
    m_check.evaluation = withPob(m_check.evaluation, *best);
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
