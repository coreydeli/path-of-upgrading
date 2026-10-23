// SPDX-License-Identifier: MIT
#include "platform/screenmap.h"

#include <algorithm>

namespace pou {

std::optional<ScreenPoint> mapToScreen(const QPoint &point, const QList<NamedScreen> &monitors,
                                       const QList<NamedScreen> &screens)
{
    const auto monitor = std::ranges::find_if(
        monitors, [&point](const NamedScreen &monitor) { return monitor.geometry.contains(point); });
    if (monitor == monitors.cend())
        return std::nullopt;
    auto screen = std::ranges::find_if(
        screens, [&monitor](const NamedScreen &screen) { return screen.name == monitor->name; });
    // older Xwayland calls its monitors XWAYLAND0 and on, and with one of
    // each it's still the same one
    if (screen == screens.cend() && monitors.size() == 1 && screens.size() == 1)
        screen = screens.cbegin();
    if (screen == screens.cend())
        return std::nullopt;

    const QPoint offset = point - monitor->geometry.topLeft();
    return ScreenPoint{.screen = screen->name,
                       .position =
                           QPoint(offset.x() * screen->geometry.width() / monitor->geometry.width(),
                                  offset.y() * screen->geometry.height() / monitor->geometry.height())};
}

} // namespace pou
