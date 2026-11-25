// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QUrl>

namespace pou {

class PoeAccount;
class TradeClient;
class TradeSession;

// Which sign in the trade client uses: the session ID you saved, or, in a
// build that can, the linked Path of Exile account. It's the one place that
// says, as the session keeps handing the client its id whenever that changes.
class TradeSignInChoice : public QObject
{
    Q_OBJECT

public:
    // They have to outlive it. The account is offered here only when the
    // build has an address to search with, and then it can be chosen.
    TradeSignInChoice(TradeClient *client, TradeSession *session, PoeAccount *account, const QUrl &base,
                      QObject *parent = nullptr);

    // Whether the account can be chosen at all: a beta that was registered
    // for it, with a scope and an address for trade searches.
    bool isOffered() const;
    // What was chosen, which is the session unless the account is offered.
    TradeSignInKind kind() const;
    void choose(TradeSignInKind kind);
    // Whether the sign in in use has someone signed in: an id saved, or an
    // account linked.
    bool isSignedIn() const;

Q_SIGNALS:
    // Whenever the choice, the saved id or the link changes, so whoever knows
    // what a signed in search can do looks again.
    void changed();

private:
    void apply();

    TradeClient *m_client;
    TradeSession *m_session;
    PoeAccount *m_account;
    QUrl m_base;
    TradeSignInKind m_chosen = TradeSignInKind::Session;
};

} // namespace pou
