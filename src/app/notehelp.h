// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringView>

namespace pou {

// What a gear note's label means, "Weak rolls", for its tooltip. Empty for a
// label it doesn't know.
QString noteHelp(QStringView label);

} // namespace pou
