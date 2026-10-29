// SPDX-License-Identifier: MIT
#include "app/itemhistory.h"

#include "core/datafiles.h"
#include "core/parser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace pou {
namespace {

constexpr int formatVersion = 1;
constexpr qsizetype recentCount = 30;

} // namespace

ItemHistory::ItemHistory(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2s);
    connect(&m_saveTimer, &QTimer::timeout, this, &ItemHistory::save);
    load();
}

ItemHistory::~ItemHistory()
{
    if (m_saveTimer.isActive())
        save();
}

void ItemHistory::setSaveDelay(std::chrono::milliseconds delay)
{
    m_saveTimer.setInterval(delay);
}

void ItemHistory::add(const Item &item)
{
    m_recent.removeIf([&](const Item &old) { return old.raw == item.raw; });
    m_recent.prepend(item);
    if (m_recent.size() > recentCount)
        m_recent.removeLast();
    m_seen.note(item);
    m_saveTimer.start();
    Q_EMIT changed();
}

void ItemHistory::clearRecent()
{
    if (m_recent.isEmpty())
        return;
    m_recent.clear();
    m_saveTimer.start();
    Q_EMIT changed();
}

bool ItemHistory::save()
{
    m_saveTimer.stop();
    QJsonArray recent;
    for (const Item &item : std::as_const(m_recent))
        recent << item.raw;
    const QJsonObject o{
        {u"version"_s, formatVersion},
        {u"recent"_s, recent},
        {u"seenStats"_s, m_seen.toJson()},
    };
    return writeJsonFile(m_path, o, &m_error);
}

void ItemHistory::load()
{
    if (!QFile::exists(m_path))
        return;
    const std::optional<QJsonObject> o = readJsonFile(m_path, &m_error);
    if (!o)
        return;
    // as the game's text, which is read again with what the parser knows now
    for (const QJsonValue &text : o->value(u"recent").toArray()) {
        if (std::optional<Item> item = parseItem(text.toString()))
            m_recent << std::move(*item);
    }
    m_seen = SeenStatsLog::fromJson(o->value(u"seenStats").toObject());
}

} // namespace pou
