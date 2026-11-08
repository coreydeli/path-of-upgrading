// SPDX-License-Identifier: MIT
#include "core/placement.h"

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace pou {

QString popupPositionId(PopupPosition position)
{
    switch (position) {
    case PopupPosition::Cursor:
        return u"cursor"_s;
    case PopupPosition::Left:
        return u"left"_s;
    case PopupPosition::Right:
        return u"right"_s;
    case PopupPosition::Top:
        return u"top"_s;
    }
    return {};
}

PopupPosition popupPositionFromId(QStringView id)
{
    for (PopupPosition position : {PopupPosition::Left, PopupPosition::Right, PopupPosition::Top}) {
        if (id == popupPositionId(position))
            return position;
    }
    return PopupPosition::Cursor;
}

namespace {

constexpr int fromSide = 16;
constexpr int fromEdge = 4;
// the gap between the popup and the mouse
constexpr int fromMouse = 20;

// How far down the windows at the sides go.
int sideTop(const QSize &screen)
{
    return screen.height() * 12 / 100;
}

} // namespace

// And in the top left corner when it's bigger than the screen.
QPoint withinScreen(QPoint at, const QSize &window, const QSize &screen)
{
    at.setX(std::max(fromEdge, std::min(at.x(), screen.width() - window.width() - fromEdge)));
    at.setY(std::max(fromEdge, std::min(at.y(), screen.height() - window.height() - fromEdge)));
    return at;
}

QPoint placePopup(PopupPosition where, const QSize &popup, const QSize &screen,
                  const std::optional<QPoint> &mouse)
{
    if (where == PopupPosition::Cursor && !mouse)
        where = PopupPosition::Top;

    QPoint at;
    switch (where) {
    case PopupPosition::Cursor: {
        const int left = mouse->x() - fromMouse - popup.width();
        const int right = mouse->x() + fromMouse;
        const bool fitsLeft = left >= fromEdge;
        const bool fitsRight = right + popup.width() <= screen.width() - fromEdge;
        // on the left unless there's no room there, and if there's none on
        // either side, where there's more
        const bool onTheLeft = fitsLeft || (!fitsRight && mouse->x() >= screen.width() - mouse->x());
        at.setX(onTheLeft ? left : right);
        at.setY(mouse->y());
        break;
    }
    case PopupPosition::Left:
        at = {fromSide, sideTop(screen)};
        break;
    case PopupPosition::Right:
        at = {screen.width() - popup.width() - fromSide, sideTop(screen)};
        break;
    case PopupPosition::Top:
        at = {(screen.width() - popup.width()) / 2, fromSide};
        break;
    }

    return withinScreen(at, popup, screen);
}

QPoint placeResults(PopupPosition popup, const QSize &window, const QSize &screen)
{
    const int x = popup == PopupPosition::Right ? fromSide : screen.width() - window.width() - fromSide;
    return withinScreen({x, sideTop(screen)}, window, screen);
}

int maxResultsHeight(const QSize &screen)
{
    return screen.height() * 76 / 100;
}

} // namespace pou
