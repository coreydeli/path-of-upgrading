// SPDX-License-Identifier: MIT
#pragma once

namespace pou {

// Makes the app's own controls style the one Qt Quick Controls uses, whatever
// the desktop's is. It's called before the QML engine is made.
void useOwnStyle();

} // namespace pou
