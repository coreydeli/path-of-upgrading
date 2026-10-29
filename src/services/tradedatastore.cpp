// SPDX-License-Identifier: MIT
#include "services/tradedatastore.h"

#include "core/datafiles.h"
#include "core/league.h"
#include "services/logging.h"
#include "services/tradeclient.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Whether what was fetched then is less than a day old. A time ahead of the
// clock counts as old, or it would never be asked for again.
bool fresh(const QDateTime &fetched, const QDateTime &now)
{
    const qint64 age = fetched.secsTo(now);
    return fetched.isValid() && age >= 0 && age < 24 * 60 * 60;
}

} // namespace

TradeDataStore::TradeDataStore(TradeClient *client, const QString &folder, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_folder(folder)
    , m_sources{{{u"stats"_s}, {u"filters"_s}, {u"leagues"_s}}}
{}

TradeDataStore::~TradeDataStore()
{
    // calls off what's still asked for
    for (const Source &source : m_sources)
        delete source.reply;
}

void TradeDataStore::load()
{
    if (m_read)
        return;
    for (Source &source : m_sources) {
        const QString file = path(source);
        if (const std::optional<QJsonObject> json = readJsonFile(file); json && take(source, *json))
            source.fetched = QFileInfo(file).lastModified().toUTC();
    }
    m_read = true;
}

void TradeDataStore::refresh(bool force)
{
    // that one's done when what's asked for comes in
    if (isRefreshing())
        return;
    load();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (Source &source : m_sources) {
        if (!force && fresh(source.fetched, now))
            continue;
        source.reply = m_client->data(source.name);
        connect(source.reply, &TradeReply::finished, this, [this, &source] { answered(source); });
    }
    if (!isRefreshing())
        QMetaObject::invokeMethod(this, &TradeDataStore::finish, Qt::QueuedConnection);
}

bool TradeDataStore::isRefreshing() const
{
    return std::any_of(m_sources.begin(), m_sources.end(),
                       [](const Source &source) { return !source.reply.isNull(); });
}

QString TradeDataStore::path(const Source &source) const
{
    return m_folder + u'/' + source.name + u".json"_s;
}

// Reads in what the site said, unless it's empty, which would leave
// searches with less than the file had.
bool TradeDataStore::take(const Source &source, const QJsonObject &answer)
{
    if (source.name == u"stats") {
        StatIndex stats = StatIndex::fromJson(answer);
        if (stats.isEmpty())
            return false;
        m_data.stats = std::move(stats);
    } else if (source.name == u"filters") {
        TradeCategories categories = TradeCategories::fromJson(answer);
        if (categories.isEmpty())
            return false;
        m_data.categories = std::move(categories);
    } else {
        QStringList leagues = tradeLeaguesFromJson(answer);
        if (leagues.isEmpty())
            return false;
        m_leagues = std::move(leagues);
    }
    return true;
}

void TradeDataStore::answered(Source &source)
{
    TradeReply *reply = source.reply;
    reply->deleteLater();
    source.reply.clear();

    const bool stats = source.name == u"stats";
    if (reply->error() != TradeReply::Error::None) {
        qCWarning(lcTrade, "Couldn't get the trade site's %s: %s", qUtf8Printable(source.name),
                  qUtf8Printable(reply->errorString()));
        if (stats)
            m_errorString = u"Searches need the trade site's list of stats. %1"_s.arg(reply->errorString());
    } else if (!take(source, reply->answer())) {
        qCWarning(lcTrade, "The trade site's %s came back empty", qUtf8Printable(source.name));
        if (stats)
            m_errorString = u"Searches need the trade site's list of stats, and it sent an empty one."_s;
    } else {
        source.fetched = QDateTime::currentDateTimeUtc();
        const QString file = path(source);
        QString error;
        if (!writeFile(file, QJsonDocument(reply->answer()).toJson(QJsonDocument::Compact), &error))
            qCWarning(lcTrade, "Couldn't keep the trade site's %s in %s: %s", qUtf8Printable(source.name),
                      qUtf8Printable(file), qUtf8Printable(error));
    }
    if (!isRefreshing())
        finish();
}

void TradeDataStore::finish()
{
    // what the files had does when the site doesn't answer
    if (!m_data.stats.isEmpty())
        m_errorString.clear();
    Q_EMIT refreshed();
}

} // namespace pou
