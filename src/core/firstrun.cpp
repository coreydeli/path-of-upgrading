// SPDX-License-Identifier: MIT
#include "core/firstrun.h"

namespace pou {

bool setupWanted(const Settings &settings, bool charactersSaved, bool beta)
{
    if (charactersSaved)
        return false;
    // Put back what a first start writes. A finished guide differs from the
    // first start too, as the settings hold setupDone.
    Settings untouched = settings;
    untouched.overlay.startChosen = false;
    untouched.overlay.hotkey = OverlaySettings().hotkey;
    untouched.overlay.resultsHotkey = OverlaySettings().resultsHotkey;
    Settings first;
    first.trade.enabled = beta;
    return untouched == first;
}

} // namespace pou
