// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace pou {

// A request to the trade site. It belongs to the client, and is yours to
// delete once it has finished.
class TradeReply : public QObject
{
    Q_OBJECT

public:
    enum class Error {
        None,
        BadAnswer, // the site's answer couldn't be read
    };
    Q_ENUM(Error)

    bool isFinished() const { return m_finished; }
    Error error() const { return m_error; }
    // What went wrong, for people.
    QString errorString() const { return m_errorString; }
    // The site's answer, when there's no error.
    QJsonObject answer() const { return m_answer; }

Q_SIGNALS:
    // Always from the event loop, never from inside a call to the client.
    void finished();

private:
    friend class TradeClient;

    explicit TradeReply(QObject *parent);
    void succeed(const QJsonObject &answer);
    void fail(Error error, const QString &why);
    void finish();

    bool m_finished = false;
    Error m_error = Error::None;
    QString m_errorString;
    QJsonObject m_answer;
};

// The official trade site.
class TradeClient : public QObject
{
    Q_OBJECT

public:
    // The network has to outlive the client.
    explicit TradeClient(QNetworkAccessManager *network, QObject *parent = nullptr);

    // A TradeQuery's body. The answer has the search's "id", how many
    // listings it found in "total" and the ids of the first 100 in "result".
    TradeReply *search(const QString &league, const QJsonObject &body);

private:
    TradeReply *send(const QNetworkRequest &request, const QByteArray &body);
    void answered(TradeReply *trade, QNetworkReply *reply);

    QNetworkAccessManager *m_network;
};

} // namespace pou
