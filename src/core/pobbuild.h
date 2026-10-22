// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <optional>

namespace pou {

// The build XML behind a Path of Building code, the text its Import/Export
// tab shares: the XML deflated and base64 encoded. XML pasted as it is
// passes through. Empty when it isn't a code.
std::optional<QString> decodeBuildCode(const QString &code);

} // namespace pou
