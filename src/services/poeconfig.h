// SPDX-License-Identifier: MIT
#pragma once

#include "core/poeoauth.h"

namespace pou {

// What this build was registered with, from the POU_POE_* CMake options.
// Empty in a build that wasn't given them.
PoeConfig builtInPoeConfig();

} // namespace pou
