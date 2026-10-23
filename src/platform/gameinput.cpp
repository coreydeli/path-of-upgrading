// SPDX-License-Identifier: MIT
#include "platform/gameinput.h"

#include "platform/xcb/xgameinput.h"

namespace pou {

std::unique_ptr<GameInput> createGameInput()
{
    return std::make_unique<XGameInput>();
}

} // namespace pou
