// SPDX-License-Identifier: MIT
#pragma once

#include <QLoggingCategory>

namespace pou {

// What the app says about the game's window and the keys it presses, for
// finding out why a copy didn't work. QT_LOGGING_RULES=
// "pathofupgrading.input.debug=true" shows it all.
Q_DECLARE_LOGGING_CATEGORY(lcInput)

} // namespace pou
