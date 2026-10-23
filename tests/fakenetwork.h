// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTest>
#include <QTimer>
#include <cstring>
#include <utility>

// A network for tests, which answers each request with the next of the
// answers it was given, or an empty JSON object once they run out, and keeps
// what was sent.

struct FakeAnswer
{
    int status = 200;
    QByteArray body = "{}";
    QList<std::pair<QByteArray, QByteArray>> headers;
    // with no status or headers, as when the site can't be reached
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
};

class FakeReply : public QNetworkReply
{
public:
    FakeReply(QNetworkAccessManager::Operation operation, const QNetworkRequest &request,
              const FakeAnswer &answer, QObject *parent)
        : QNetworkReply(parent)
        , m_body(answer.body)
    {
        setOperation(operation);
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        // a moment later, as an answer never comes before the request is out
        QTimer::singleShot(0, this, [this, answer] {
            if (answer.error == NoError) {
                setAttribute(QNetworkRequest::HttpStatusCodeAttribute, answer.status);
                for (const auto &[name, value] : answer.headers)
                    setRawHeader(name, value);
            } else {
                setError(answer.error, QStringLiteral("Host %1 not found").arg(url().host()));
                Q_EMIT errorOccurred(answer.error);
            }
            setFinished(true);
            const QPointer<FakeReply> alive(this);
            m_finishing = true;
            Q_EMIT finished();
            if (alive)
                m_finishing = false;
        });
    }

    ~FakeReply() override
    {
        // which a real reply can't take
        if (m_finishing)
            QTest::qFail("A reply was deleted in a slot connected to its finished signal", __FILE__,
                         __LINE__);
    }

    void abort() override {}
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override
    {
        return m_body.size() - m_read + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 n = std::min(maxSize, m_body.size() - m_read);
        std::memcpy(data, m_body.constData() + m_read, size_t(n));
        m_read += n;
        return n;
    }

private:
    QByteArray m_body;
    qint64 m_read = 0;
    bool m_finishing = false;
};

class FakeNetwork : public QNetworkAccessManager
{
public:
    struct Sent
    {
        Operation operation;
        QNetworkRequest request;
        QByteArray body;
        QPointer<QNetworkReply> reply; // null once it's deleted
    };

    QList<FakeAnswer> answers;
    QList<Sent> sent;

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        auto *reply =
            new FakeReply(operation, request, answers.isEmpty() ? FakeAnswer() : answers.takeFirst(), this);
        sent << Sent{operation, request, outgoingData ? outgoingData->readAll() : QByteArray(), reply};
        return reply;
    }
};
