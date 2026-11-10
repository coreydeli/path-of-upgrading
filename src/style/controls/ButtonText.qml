// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import PouStyle

// A button's words, in small capitals.
Text {
    id: words

    required property T.AbstractButton control
    readonly property bool primary: control.highlighted || control.checked

    font: Look.labelFont(control.font)
    text: control.text
    textFormat: Text.PlainText
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
    opacity: control.enabled ? 1 : 0.4
    color: primary || control.down ? Kirigami.Theme.backgroundColor : Kirigami.Theme.textColor
}
