// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <optional>

namespace pou {

// A monitor, as the X server or Qt has it.
struct NamedScreen
{
    QString name;
    QRect geometry;
};

// A point on one of Qt's screens, from its top left.
struct ScreenPoint
{
    QString screen;
    QPoint position;

    friend bool operator==(const ScreenPoint &, const ScreenPoint &) = default;
};

// Where a point on the X server's monitors is on Qt's screens. They're
// matched by name, like DP-1, and the point is scaled from one's size to the
// other's: when X11 programs scale themselves, the X server counts in real
// pixels and Qt in scaled ones.
std::optional<ScreenPoint> mapToScreen(const QPoint &point, const QList<NamedScreen> &monitors,
                                       const QList<NamedScreen> &screens);

} // namespace pou
