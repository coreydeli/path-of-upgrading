// SPDX-License-Identifier: MIT
#pragma once

#include <QPoint>
#include <QSize>
#include <QString>
#include <optional>

namespace pou {

enum class PopupPosition { Cursor, Left, Right, Top };

// As the settings keep it: "cursor", "left", "right" or "top".
QString popupPositionId(PopupPosition position);
// Cursor for anything unknown.
PopupPosition popupPositionFromId(QStringView id);

// Where the popup's top left goes on the screen the mouse is on, counted from
// the screen's top left. Next to the mouse it goes on the right, or on the
// left when there's no room there, and at the top when we don't know where
// the mouse is. It stays a few pixels in from the edges.
QPoint placePopup(PopupPosition where, const QSize &popup, const QSize &screen,
                  const std::optional<QPoint> &mouse);

} // namespace pou
