// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import PouStyle

// Material's state layer: the control's content color laid over it, a little
// when it's hovered or has the keys and a little more when it's pressed.
Rectangle {
    id: layer

    required property T.Control control
    property color tone: "black"
    property bool pressed: false

    color: tone
    opacity: !control.enabled ? 0 : pressed ? 0.1 : control.visualFocus ? 0.1 : control.hovered ? 0.08 : 0

    Behavior on opacity {
        NumberAnimation {
            duration: Look.shortDuration / 2
        }
    }
}
