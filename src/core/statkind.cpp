// SPDX-License-Identifier: MIT
#include "core/statkind.h"

#include <QRegularExpression>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

bool matches(const QString &text, const QRegularExpression &words)
{
    return words.match(text).hasMatch();
}

} // namespace

StatKind statKindOf(const QString &text)
{
    static const QRegularExpression penetrates(uR"(\bpenetrat)"_s, QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression resistance(uR"(\bresist)"_s, QRegularExpression::CaseInsensitiveOption);
    // Checked before damage, so "Damage taken" and "Block" count as defences.
    static const QRegularExpression defence(
        uR"(\b(life|energy shield|armou?r|evasion|block|deflect\w*|stun|ailment threshold|damage taken|damage reduction)\b)"_s,
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression damage(
        uR"(\b(damage|critical|attack|attacks|cast|spell|spells|accuracy|projectile|projectiles|skill|skills)\b)"_s,
        QRegularExpression::CaseInsensitiveOption);

    if (matches(text, penetrates))
        return StatKind::Damage;
    if (matches(text, resistance))
        return StatKind::Resistance;
    if (matches(text, defence))
        return StatKind::Defence;
    if (matches(text, damage))
        return StatKind::Damage;
    return StatKind::Other;
}

} // namespace pou
