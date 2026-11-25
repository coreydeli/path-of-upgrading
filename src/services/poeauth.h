// SPDX-License-Identifier: MIT
#pragma once

#include "core/poeoauth.h"
#include "services/loopbackserver.h"

#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <chrono>
#include <memory>

class QNetworkAccessManager;

namespace pou {

// Links a Path of Exile account: starts listening on this computer, asks for
// the browser to be opened on the sign in, takes the code the browser hands
// back and trades it for tokens. It also trades a refresh token for new ones.
// It doesn't open the browser or keep the tokens, those are the caller's.
class PoeAuth : public QObject
{
    Q_OBJECT

public:
    PoeAuth(QNetworkAccessManager &network, PoeConfig config, QObject *parent = nullptr);

    // How long to wait for the browser, three minutes at first.
    void setWaitLimit(std::chrono::milliseconds limit) { m_waitLimit = limit; }
    // Where the tokens come from, for a test's own server.
    void setTokenUrl(const QUrl &url) { m_tokenUrl = url; }

    bool isWaiting() const { return m_state != nullptr; }

    // Starts a sign in, calling off one that's waiting.
    void start();
    // Stops waiting, with nothing said.
    void cancel();
    void refresh(const QString &refreshToken);

Q_SIGNALS:
    // Open this in the browser.
    void browserWanted(const QUrl &url);
    // The player allowed it.
    void linked(const pou::PoeTokens &tokens);
    void refreshed(const pou::PoeTokens &tokens);
    // Why not, for the player. newLinkNeeded when only linking again helps.
    void failed(const QString &text, bool newLinkNeeded);

private:
    // What a sign in that's waiting needs to finish it.
    struct Waiting
    {
        QString state;
        QString verifier;
        quint16 port = 0;
    };

    void answered(const QUrl &url);
    void fail(const QString &text, bool newLinkNeeded = false);
    QNetworkReply *post(const QByteArray &body);
    void takeTokens(QNetworkReply *reply, bool signIn);

    QNetworkAccessManager &m_network;
    PoeConfig m_config;
    QUrl m_tokenUrl;
    std::chrono::milliseconds m_waitLimit = std::chrono::minutes(3);
    LoopbackServer m_server;
    QTimer m_timer;
    std::unique_ptr<Waiting> m_state;
};

} // namespace pou
