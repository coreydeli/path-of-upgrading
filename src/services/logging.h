// SPDX-License-Identifier: MIT
#pragma once

#include <QLoggingCategory>

namespace pou {

// What the app says about the sites it talks to, for finding out why
// something didn't work. Warnings and notes show by default, the rest with
// QT_LOGGING_RULES="pathofupgrading.*.debug=true".
Q_DECLARE_LOGGING_CATEGORY(lcTrade)

} // namespace pou
