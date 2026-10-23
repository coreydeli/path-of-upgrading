// SPDX-License-Identifier: MIT
#include "core/tradesite.h"
#include "fakenetwork.h"
#include "services/network.h"
#include "services/tradeclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

QByteArray json(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool finishes(TradeReply *reply)
{
    QSignalSpy finished(reply, &TradeReply::finished);
    return finished.wait();
}

} // namespace

class TestTradeClient : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void searches()
    {
        const QJsonObject found{
            {u"id"_s, u"Ab3dE9"_s},
            {u"total"_s, 2},
            {u"result"_s, QJsonArray{u"x1"_s, u"x2"_s}},
        };
        FakeNetwork network;
        network.answers << FakeAnswer{.body = json(found)};
        TradeClient client(&network);

        const QJsonObject body{{u"query"_s, QJsonObject{{u"type"_s, u"Sapphire Ring"_s}}}};
        TradeReply *reply = client.search(u"Runes of Aldur"_s, body);
        QVERIFY(finishes(reply));
        QCOMPARE(reply->error(), TradeReply::Error::None);
        QCOMPARE(reply->answer(), found);

        QCOMPARE(network.sent.size(), 1);
        const FakeNetwork::Sent &sent = network.sent.first();
        QCOMPARE(sent.operation, QNetworkAccessManager::PostOperation);
        QCOMPARE(sent.request.url(), tradeSearchUrl(u"Runes of Aldur"_s));
        QCOMPARE(sent.request.header(QNetworkRequest::ContentTypeHeader).toByteArray(), "application/json");
        QCOMPARE(sent.request.header(QNetworkRequest::UserAgentHeader).toByteArray(), userAgent());
        QCOMPARE(QJsonDocument::fromJson(sent.body).object(), body);

        // the network's reply goes once it has been read
        QTRY_VERIFY(sent.reply.isNull());
    }
};

QTEST_GUILESS_MAIN(TestTradeClient)
#include "tst_tradeclient.moc"
