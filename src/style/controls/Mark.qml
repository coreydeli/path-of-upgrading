// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import PouStyle

// The box a switch, a check box or a radio button shows. A switch has two
// cells, Off and On, with the one that's set filled in; a check box is a box
// and a radio button a diamond.
Item {
    id: mark

    required property T.AbstractButton control
    property bool round: false
    property bool twoCells: false

    readonly property bool on: control.checked
    readonly property color ink: Kirigami.Theme.textColor
    readonly property color ground: Kirigami.Theme.backgroundColor

    implicitWidth: twoCells ? cells.implicitWidth : Kirigami.Units.gridUnit
    implicitHeight: Kirigami.Units.gridUnit
    opacity: control.enabled ? 1 : 0.4

    Row {
        id: cells

        visible: mark.twoCells
        anchors.centerIn: parent

        Repeater {
            model: [qsTr("Off"), qsTr("On")]

            Rectangle {
                required property string modelData
                required property int index
                readonly property bool lit: (index === 1) === mark.on

                width: word.implicitWidth + 2 * Kirigami.Units.smallSpacing + 2
                height: Kirigami.Units.gridUnit
                color: lit ? (index ? Kirigami.Theme.highlightColor : mark.ink) : "transparent"
                border.width: 1
                border.color: Look.tint(mark.ground, mark.ink, 0.6)

                Text {
                    id: word

                    anchors.centerIn: parent
                    font: Look.labelFont(Kirigami.Theme.smallFont)
                    text: parent.modelData
                    color: parent.lit ? mark.ground : Look.tint(mark.ground, mark.ink, 0.55)
                }
            }
        }
    }

    Rectangle {
        visible: !mark.twoCells
        anchors.centerIn: parent
        width: Math.round(Kirigami.Units.gridUnit * 0.7)
        height: width
        rotation: mark.round ? 45 : 0
        antialiasing: true
        color: mark.on ? Kirigami.Theme.highlightColor : "transparent"
        border.width: 1.5
        border.color: mark.on ? Kirigami.Theme.highlightColor : Look.tint(mark.ground, mark.ink, 0.6)
    }
}
