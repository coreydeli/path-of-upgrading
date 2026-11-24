// SPDX-License-Identifier: MIT
#include "app/statkinds.h"

namespace pou {

ChipKind::Kind StatKinds::of(const QString &text)
{
    return static_cast<ChipKind::Kind>(statKindOf(text));
}

} // namespace pou
