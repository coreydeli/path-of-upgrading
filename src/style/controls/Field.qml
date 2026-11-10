// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigami as Kirigami
import PouStyle

// Where you type: a paper box with ink in it, and a teal edge while it has
// the keys.
Rectangle {
    id: field

    required property Item control

    implicitWidth: 8 * Kirigami.Units.gridUnit
    implicitHeight: Kirigami.Units.gridUnit + 2 * Kirigami.Units.smallSpacing
    color: Look.paper
    border.width: control.activeFocus ? 2 : 0
    border.color: Kirigami.Theme.highlightColor
}
