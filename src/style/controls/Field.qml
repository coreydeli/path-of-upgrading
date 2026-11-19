// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigami as Kirigami
import PouStyle

// Where you type: a paper box with ink in it, and a teal edge while it has
// the keys. In Material it's an outlined box, with a primary outline and a
// thicker one while it has the keys.
Rectangle {
    id: field

    required property Item control

    implicitWidth: 8 * Kirigami.Units.gridUnit
    implicitHeight: Look.flat ? 40 * Look.dp : Kirigami.Units.gridUnit + 2 * Kirigami.Units.smallSpacing
    color: Look.flat ? "transparent" : Look.paper
    radius: Look.flat ? Look.shapeExtraSmall : 0
    border.width: Look.flat ? (control.activeFocus ? 2 : 1) : control.activeFocus ? 2 : 0
    border.color: Look.flat && !control.activeFocus ? Look.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, control.hovered ? 0.8 : 0.5)
                                                       : Kirigami.Theme.highlightColor
}
