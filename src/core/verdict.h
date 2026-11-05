// SPDX-License-Identifier: MIT
#pragma once

namespace pou {

// What swapping in an item would mean for you.
enum class Verdict {
    New, // nothing saved in that slot to compare with
    Same,
    Small, // better, but not by much
    Upgrade,
    Downgrade,
    Risky,       // better, but it breaks something: Spirit, attributes...
    ResistLoss,  // a resistance drops off the cap or further under it
    DamageLoss,  // costs a good part of your main damage
    DefenceLoss, // costs a good part of your effective HP, Life or Energy Shield
    Equipped,    // the copied item is the one you're wearing
};

} // namespace pou
