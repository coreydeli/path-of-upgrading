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

// Where the results window's top left goes: at the right as far down as the
// popup is when it's at a side, or at the left when the popup is at the
// right. It stays a few pixels in from the edges.
QPoint placeResults(PopupPosition popup, const QSize &window, const QSize &screen);
// The most height the results window can have, which leaves room above and
// below it.
int maxResultsHeight(const QSize &screen);

} // namespace pou
