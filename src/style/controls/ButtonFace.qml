// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import PouStyle

// What every button is drawn on: a ruled box, filled in ink when it's the one
// to press and in teal while it's pressed.
Item {
    id: face

    required property T.AbstractButton control
    property bool flat: false

    readonly property bool lit: control.hovered || control.visualFocus
    readonly property bool primary: control.highlighted || control.checked
    readonly property color ink: Kirigami.Theme.textColor
    readonly property color ground: Kirigami.Theme.backgroundColor

    implicitWidth: 3 * Kirigami.Units.gridUnit
    implicitHeight: Kirigami.Units.gridUnit + 2 * Kirigami.Units.smallSpacing

    Rectangle {
        anchors.fill: parent
        color: {
            if (face.control.down)
                return Kirigami.Theme.highlightColor;
            if (face.primary)
                return face.ink;
            return face.lit ? Look.tint(face.ground, Kirigami.Theme.highlightColor, 0.18) : "transparent";
        }
        border.width: face.flat && !face.lit && !face.primary ? 0 : 1
        border.color: face.lit && !face.primary ? Kirigami.Theme.highlightColor : Look.tint(face.ground, face.ink, 0.6)
    }
}
