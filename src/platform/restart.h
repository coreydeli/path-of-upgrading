// SPDX-License-Identifier: MIT
#pragma once

namespace pou {

// Starts this program again, once this one has gone: it waits for the process
// to end, so the new one isn't turned away as a second copy. The caller quits.
// False when it couldn't be started.
bool startAgainAfterExit();

} // namespace pou
