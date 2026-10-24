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

QPoint placePopup(PopupPosition where, const QSize &popup, const QSize &screen,
                  const std::optional<QPoint> &mouse)
{
    constexpr int fromMouse = 24;
    constexpr int fromSide = 16;
    constexpr int fromEdge = 4;

    if (where == PopupPosition::Cursor && !mouse)
        where = PopupPosition::Top;

    QPoint at;
    switch (where) {
    case PopupPosition::Cursor:
        at.setX(mouse->x() + fromMouse);
        if (at.x() + popup.width() > screen.width())
            at.setX(mouse->x() - popup.width() - fromMouse);
        // with the mouse a third of the way down it
        at.setY(mouse->y() - popup.height() / 3);
        break;
    case PopupPosition::Left:
        at = {fromSide, screen.height() * 12 / 100};
        break;
    case PopupPosition::Right:
        at = {screen.width() - popup.width() - fromSide, screen.height() * 12 / 100};
        break;
    case PopupPosition::Top:
        at = {(screen.width() - popup.width()) / 2, fromSide};
        break;
    }

    // and in the top left corner when it's bigger than the screen
    at.setX(std::max(fromEdge, std::min(at.x(), screen.width() - popup.width() - fromEdge)));
    at.setY(std::max(fromEdge, std::min(at.y(), screen.height() - popup.height() - fromEdge)));
    return at;
}

} // namespace pou
