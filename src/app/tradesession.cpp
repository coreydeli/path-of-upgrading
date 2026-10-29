// SPDX-License-Identifier: MIT
#include "app/tradesession.h"

#include "platform/secretstore.h"
#include "services/tradeclient.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// What it's kept under, as the site calls the cookie.
const QString secretName = u"POESESSID"_s;

} // namespace

TradeSession::TradeSession(SecretStore *store, TradeClient *client, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_client(client)
{}

QString TradeSession::status() const
{
    if (m_busy)
        return u"Waiting for the keyring."_s;
    switch (m_state) {
    case State::NotAsked:
        return u"The keyring is only asked while trade searches are on."_s;
    case State::Unknown:
        return u"Couldn't tell if one is saved."_s;
    case State::SignedOut:
        return u"No session ID is saved."_s;
    case State::SignedIn:
        break;
    }
    return u"A session ID is saved."_s;
}

void TradeSession::load()
{
    // once, so that a keyring that said no isn't asked again at every change
    // of the settings
    if (m_busy || m_state != State::NotAsked)
        return;
    begin();
    m_store->read(secretName, this, [this](const SecretStore::Result &result) {
        switch (result.status) {
        case SecretStore::Status::Done:
            signIn(result.secret);
            break;
        case SecretStore::Status::NotFound:
            signIn({});
            break;
        case SecretStore::Status::Failed:
            m_state = State::Unknown;
            m_error = result.error;
            break;
        }
        end();
    });
}

void TradeSession::save(const QString &text)
{
    if (m_busy)
        return;
    const QString id = text.trimmed();
    if (!TradeClient::isSessionId(id)) {
        m_error = u"That isn't a session ID. It's only letters and digits."_s;
        Q_EMIT changed();
        return;
    }
    begin();
    m_store->write(secretName, id, this, [this, id](const SecretStore::Result &result) {
        if (result.status == SecretStore::Status::Done) {
            signIn(id);
            Q_EMIT stored();
        } else {
            m_error = result.error;
        }
        end();
    });
}

void TradeSession::forget()
{
    if (m_busy)
        return;
    begin();
    m_store->remove(secretName, this, [this](const SecretStore::Result &result) {
        // there being none is as good
        if (result.status == SecretStore::Status::Failed)
            m_error = result.error;
        else
            signIn({});
        end();
    });
}

void TradeSession::begin()
{
    m_busy = true;
    m_error.clear();
    Q_EMIT changed();
}

void TradeSession::end()
{
    m_busy = false;
    Q_EMIT changed();
}

void TradeSession::signIn(const QString &id)
{
    // what isn't one signs out, as the client would
    const QString used = TradeClient::isSessionId(id.trimmed()) ? id.trimmed() : QString();
    m_state = used.isEmpty() ? State::SignedOut : State::SignedIn;
    m_client->setSessionId(used);
    if (used != m_id) {
        m_id = used;
        Q_EMIT idChanged();
    }
}

} // namespace pou
