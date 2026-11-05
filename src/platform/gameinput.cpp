// SPDX-License-Identifier: MIT
#include "platform/gameinput.h"

#ifdef Q_OS_WIN
#include "platform/win/wingameinput.h"
#else
#include "platform/xcb/xgameinput.h"
#endif

namespace pou {

std::unique_ptr<GameInput> createGameInput()
{
#ifdef Q_OS_WIN
    return std::make_unique<WinGameInput>();
#else
    return std::make_unique<XGameInput>();
#endif
}

} // namespace pou
