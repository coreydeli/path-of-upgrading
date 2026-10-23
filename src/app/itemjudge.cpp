// SPDX-License-Identifier: MIT
#include "app/itemjudge.h"

namespace pou {

ItemJudge::ItemJudge(QObject *parent)
    : QObject(parent)
{}

void ItemJudge::setCharacter(const Character &character)
{
    m_character = character;
}

void ItemJudge::setPriorities(const PobPriorities &priorities)
{
    m_priorities = priorities;
}

void ItemJudge::check(const Item &item)
{
    m_check = {.item = item, .evaluation = evaluate(item, m_character, m_priorities)};
    Q_EMIT checked(m_check);
}

} // namespace pou
