// SPDX-License-Identifier: MIT
#include "platform/secretstore.h"

#include "platform/keychain/keychainsecretstore.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {

std::unique_ptr<SecretStore> createSecretStore()
{
    return std::make_unique<KeychainSecretStore>(u"path-of-upgrading"_s);
}

} // namespace pou
