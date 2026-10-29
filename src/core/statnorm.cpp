// SPDX-License-Identifier: MIT
#include "core/statnorm.h"

#include <QRegularExpression>

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString normalizeStat(const QString &text)
{
    static const QRegularExpression filler(uR"(\b(total|to)\b)"_s);
    static const QRegularExpression local(uR"(\s*\(local\)\s*)"_s);
    static const QRegularExpression spaces(uR"(\s+)"_s);

    QString s = text.toLower();
    s.remove(u'+');
    s.replace(filler, u" "_s);
    s.replace(local, u" "_s);
    s.replace(spaces, u" "_s);
    return s.trimmed();
}

bool isConditionalStat(const QString &text)
{
    static const QRegularExpression conditional(
        uR"(\b(if you've|if you have|while|when|recently)\b|\bper (?!second))"_s,
        QRegularExpression::CaseInsensitiveOption);
    return conditional.match(text).hasMatch();
}

bool isItemProperty(QStringView key)
{
    return key.startsWith(u"[Base] ") || key.startsWith(u"[Weapon] ");
}

QString statName(const QString &key)
{
    return isItemProperty(key) ? key.sliced(key.indexOf(u"] ") + 2) : key;
}

} // namespace pou
