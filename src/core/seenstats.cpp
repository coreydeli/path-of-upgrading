// SPDX-License-Identifier: MIT
#include "core/seenstats.h"

#include <QCryptographicHash>
#include <QJsonArray>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

constexpr qsizetype remembered = 2000;

// The same for an item whether it was copied plainly or with Ctrl+Alt+C,
// which adds the mods' tiers and roll ranges to the text.
QByteArray fingerprint(const Item &item)
{
    QString text = item.itemClass + u'\n' + item.rarity + u'\n' + item.name + u'\n' + item.baseType;
    for (const auto &[key, value] : item.stats)
        text += u'\n' + key + u'=' + QString::number(value);
    return QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1)
        .toBase64(QByteArray::OmitTrailingEquals);
}

} // namespace

bool SeenStatsLog::note(const Item &item)
{
    if (item.itemClass.isEmpty() || item.stats.isEmpty())
        return false;
    const QByteArray print = fingerprint(item);
    if (m_counted.contains(print))
        return false;
    remember(print);
    QHash<QString, int> &counts = m_stats[item.itemClass];
    for (const auto &[key, value] : item.stats)
        ++counts[key];
    return true;
}

void SeenStatsLog::remember(const QByteArray &fingerprint)
{
    m_recent << fingerprint;
    m_counted.insert(fingerprint);
    if (m_recent.size() > remembered)
        m_counted.remove(m_recent.takeFirst());
}

QJsonObject SeenStatsLog::toJson() const
{
    QJsonObject byClass;
    for (auto cls = m_stats.cbegin(); cls != m_stats.cend(); ++cls) {
        QJsonObject counts;
        for (auto it = cls.value().cbegin(); it != cls.value().cend(); ++it)
            counts.insert(it.key(), it.value());
        byClass.insert(cls.key(), counts);
    }
    QJsonArray items;
    for (const QByteArray &print : m_recent)
        items << QString::fromLatin1(print);
    return {{u"byClass"_s, byClass}, {u"items"_s, items}};
}

SeenStatsLog SeenStatsLog::fromJson(const QJsonObject &o)
{
    SeenStatsLog log;
    const QJsonObject byClass = o.value(u"byClass").toObject();
    for (auto cls = byClass.constBegin(); cls != byClass.constEnd(); ++cls) {
        const QJsonObject counts = cls.value().toObject();
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
            if (const int n = it.value().toInt(); n > 0)
                log.m_stats[cls.key()].insert(it.key(), n);
        }
    }
    for (const QJsonValue &print : o.value(u"items").toArray()) {
        if (print.isString())
            log.remember(print.toString().toLatin1());
    }
    return log;
}

} // namespace pou
