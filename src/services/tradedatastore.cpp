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

void TradeDataStore::refresh()
{
    // that one's done when what's asked for comes in
    if (isRefreshing())
        return;
    for (Source &source : m_sources) {
        if (!m_read) {
            const QString file = path(source);
            if (const std::optional<QJsonObject> json = readJsonFile(file)) {
                take(source, *json);
                source.fetched = QFileInfo(file).lastModified().toUTC();
            }
        }
        if (source.fetched.isValid())
            continue;
        source.reply = m_client->data(source.name);
        connect(source.reply, &TradeReply::finished, this, [this, &source] { answered(source); });
    }
    m_read = true;
    if (!isRefreshing())
        QMetaObject::invokeMethod(this, &TradeDataStore::refreshed, Qt::QueuedConnection);
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

void TradeDataStore::take(const Source &source, const QJsonObject &answer)
{
    if (source.name == u"stats")
        m_data.stats = StatIndex::fromJson(answer);
    else if (source.name == u"filters")
        m_data.categories = TradeCategories::fromJson(answer);
    else
        m_leagues = tradeLeaguesFromJson(answer);
}

void TradeDataStore::answered(Source &source)
{
    TradeReply *reply = source.reply;
    reply->deleteLater();
    source.reply.clear();

    if (reply->error() != TradeReply::Error::None) {
        qCWarning(lcTrade, "Couldn't get the trade site's %s: %s", qUtf8Printable(source.name),
                  qUtf8Printable(reply->errorString()));
    } else {
        take(source, reply->answer());
        source.fetched = QDateTime::currentDateTimeUtc();
        const QString file = path(source);
        QString error;
        if (!writeFile(file, QJsonDocument(reply->answer()).toJson(QJsonDocument::Compact), &error))
            qCWarning(lcTrade, "Couldn't keep the trade site's %s in %s: %s", qUtf8Printable(source.name),
                      qUtf8Printable(file), qUtf8Printable(error));
    }
    if (!isRefreshing())
        Q_EMIT refreshed();
}

} // namespace pou
