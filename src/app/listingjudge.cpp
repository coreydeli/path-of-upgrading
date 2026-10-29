// SPDX-License-Identifier: MIT
#include "app/listingjudge.h"

#include "core/pobeval.h"
#include "services/pobengine.h"

#include <memory>
#include <utility>

namespace pou {
namespace {

// how many Path of Building is asked about at once
constexpr qsizetype askedAtOnce = 5;

} // namespace

ListingJudge::ListingJudge(QObject *parent)
    : QObject(parent)
{}

ListingJudge::~ListingJudge()
{
    delete m_reply;
}

void ListingJudge::setCharacter(const Character &character, const std::optional<PobBuild> &build)
{
    m_judging.character = character;
    m_judging.build = build;
    judgeAll();
}

void ListingJudge::setPob(PobEngine *pob)
{
    m_pob = pob;
    m_judging.engine = m_settings.enabled ? m_pob : nullptr;
    judgeAll();
}

void ListingJudge::setSettings(const PobSettings &settings)
{
    if (settings == m_settings)
        return;
    m_settings = settings;
    m_judging.engine = m_settings.enabled ? m_pob : nullptr;
    m_judging.options.priorities = settings.priorities;
    m_judging.options.damageWeight = settings.damageWeight;
    judgeAll();
}

void ListingJudge::add(const QList<Item> &items)
{
    for (const Item &item : items)
        m_checks << firstCheck(item, m_judging);
    askPob();
}

void ListingJudge::clear()
{
    delete m_reply;
    m_asked.clear();
    m_checks.clear();
}

void ListingJudge::judgeAll()
{
    // what it was asked is for how they were judged before
    delete m_reply;
    m_asked.clear();
    for (ItemCheck &check : m_checks)
        check = firstCheck(check.item, m_judging);
    if (!m_checks.isEmpty())
        Q_EMIT checked(0, m_checks.size() - 1);
    askPob();
}

// About the next few it hasn't heard about, unless it's waiting to.
void ListingJudge::askPob()
{
    if (m_reply)
        return;
    QList<PobItem> items;
    for (qsizetype i = 0; i < m_checks.size() && m_asked.size() < askedAtOnce; ++i) {
        if (m_checks.at(i).pob == ItemCheck::Pob::Checking) {
            m_asked << i;
            items << PobItem{.text = m_checks.at(i).item.raw};
        }
    }
    if (items.isEmpty())
        return;
    m_reply = m_judging.engine->evaluate(items);
    connect(m_reply, &PobReply::finished, this, &ListingJudge::answered);
}

void ListingJudge::answered()
{
    const std::unique_ptr<PobReply> reply(m_reply.data());
    m_reply.clear();
    const QList<qsizetype> asked = std::exchange(m_asked, {});

    const PobEval eval = reply->errorString().isEmpty() ? readPobEval(reply->answer()) : PobEval();
    for (qsizetype n = 0; n < asked.size(); ++n) {
        ItemCheck &check = m_checks[asked.at(n)];
        if (!reply->errorString().isEmpty()) {
            check.pob = ItemCheck::Pob::Failed;
            check.pobError = reply->errorString();
        } else {
            check = withPobAnswer(check, eval.before, eval.items.value(n), m_judging);
        }
    }
    Q_EMIT checked(asked.first(), asked.last());
    askPob();
}

} // namespace pou
