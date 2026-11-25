// SPDX-License-Identifier: MIT
#include "app/tradesigninchoice.h"

#include "app/poeaccount.h"
#include "app/settingsstore.h"
#include "app/tradesession.h"
#include "services/tradeclient.h"
#include "services/tradesignin.h"

#include <memory>

namespace pou {

using namespace Qt::Literals::StringLiterals;

TradeSignInChoice::TradeSignInChoice(SettingsStore *settings, TradeClient *client, TradeSession *session,
                                     PoeAccount *account, const QUrl &base, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_client(client)
    , m_session(session)
    , m_account(account)
    , m_base(base)
{
    // the session hands the client its id when it changes, which this puts
    // right again when the account is the one chosen
    connect(m_session, &TradeSession::idChanged, this, [this] {
        apply();
        Q_EMIT changed();
    });
    connect(m_account, &PoeAccount::linkedChanged, this, [this] { Q_EMIT changed(); });
}

bool TradeSignInChoice::isOffered() const
{
    return m_account->isOffered() && m_base.isValid() && !m_base.isEmpty();
}

TradeSignInKind TradeSignInChoice::kind() const
{
    return isOffered() ? m_chosen : TradeSignInKind::Session;
}

void TradeSignInChoice::choose(TradeSignInKind kind)
{
    if (kind == m_chosen)
        return;
    m_chosen = kind;
    apply();
    Q_EMIT changed();
}

void TradeSignInChoice::setUseAccount(bool useAccount)
{
    if (!isOffered())
        return;
    // which comes back through choose() when the settings change
    Settings settings = m_settings->settings();
    settings.trade.signIn = useAccount ? TradeSignInKind::Account : TradeSignInKind::Session;
    m_settings->set(settings);
}

QString TradeSignInChoice::status() const
{
    if (!useAccount())
        return u"Searches are made with your session ID."_s;
    if (m_account->isLinked())
        return u"Searches are made as your linked Path of Exile account."_s;
    return u"Your Path of Exile account isn't linked yet. Link it on the Characters page."_s;
}

bool TradeSignInChoice::isSignedIn() const
{
    return kind() == TradeSignInKind::Account ? m_account->isLinked() : m_session->isSaved();
}

void TradeSignInChoice::apply()
{
    if (kind() == TradeSignInKind::Account)
        m_client->setSignIn(std::make_unique<BearerSignIn>(m_account, m_base));
    else
        m_session->useSessionId();
}

} // namespace pou
