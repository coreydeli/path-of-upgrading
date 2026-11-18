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

    implicitWidth: Look.material ? (twoCells ? 52 : 40) * Look.dp : twoCells ? cells.implicitWidth : Kirigami.Units.gridUnit
    implicitHeight: Look.material ? (twoCells ? 32 : 40) * Look.dp : Kirigami.Units.gridUnit
    opacity: control.enabled ? 1 : 0.4

    Row {
        id: cells

        visible: mark.twoCells && !Look.material
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
        visible: !mark.twoCells && !Look.material
        anchors.centerIn: parent
        width: Math.round(Kirigami.Units.gridUnit * 0.7)
        height: width
        rotation: mark.round ? 45 : 0
        antialiasing: true
        color: mark.on ? Kirigami.Theme.highlightColor : "transparent"
        border.width: 1.5
        border.color: mark.on ? Kirigami.Theme.highlightColor : Look.tint(mark.ground, mark.ink, 0.6)
    }

    // Material's switch: a track with a thumb that grows when it's on
    Rectangle {
        id: track

        visible: Look.material && mark.twoCells
        anchors.centerIn: parent
        width: 52 * Look.dp
        height: 32 * Look.dp
        radius: height / 2
        color: mark.on ? Kirigami.Theme.highlightColor : Look.tint(mark.ground, mark.ink, 0.1)
        border.width: mark.on ? 0 : 2 * Look.dp
        border.color: Look.tint(mark.ground, mark.ink, 0.5)

        Behavior on color {
            ColorAnimation {
                duration: Look.shortDuration
            }
        }

        StateLayer {
            x: thumb.x + (thumb.width - width) / 2
            y: thumb.y + (thumb.height - height) / 2
            width: 40 * Look.dp
            height: width
            radius: width / 2
            control: mark.control
            pressed: mark.control.down
            tone: mark.on ? Kirigami.Theme.highlightColor : mark.ink
        }

        Rectangle {
            id: thumb

            property real size: (mark.on ? 24 : 16) * Look.dp

            width: size
            height: size
            radius: size / 2
            x: (mark.on ? 36 : 16) * Look.dp - size / 2
            y: (parent.height - size) / 2
            color: mark.on ? Kirigami.Theme.highlightedTextColor : Look.tint(mark.ground, mark.ink, 0.5)

            Behavior on x {
                NumberAnimation {
                    duration: Look.shortDuration
                    easing: Look.standardEasing
                }
            }

            Behavior on size {
                NumberAnimation {
                    duration: Look.shortDuration
                }
            }
        }
    }

    // Material's check box and radio button: an 18 dp box or ring in a round
    // state layer
    Rectangle {
        id: box

        visible: Look.material && !mark.twoCells
        anchors.centerIn: parent
        width: 18 * Look.dp
        height: width
        radius: mark.round ? width / 2 : 2 * Look.dp
        color: mark.on && !mark.round ? Kirigami.Theme.highlightColor : "transparent"
        border.width: mark.on && !mark.round ? 0 : 2 * Look.dp
        border.color: mark.on ? Kirigami.Theme.highlightColor : Look.tint(mark.ground, mark.ink, 0.7)

        StateLayer {
            anchors.centerIn: parent
            width: 40 * Look.dp
            height: width
            radius: width / 2
            control: mark.control
            pressed: mark.control.down
            tone: mark.on ? Kirigami.Theme.highlightColor : mark.ink
        }

        Rectangle {
            visible: mark.round
            anchors.centerIn: parent
            width: 10 * Look.dp
            height: width
            radius: width / 2
            color: Kirigami.Theme.highlightColor
            scale: mark.on ? 1 : 0

            Behavior on scale {
                NumberAnimation {
                    duration: Look.shortDuration
                    easing: Look.standardEasing
                }
            }
        }

        // the tick, two strokes
        Item {
            visible: mark.on && !mark.round
            anchors.centerIn: parent
            width: 12 * Look.dp
            height: width

            Rectangle {
                x: 1.5 * Look.dp
                y: 6.5 * Look.dp
                width: 5 * Look.dp
                height: 2 * Look.dp
                rotation: 45
                antialiasing: true
                color: Kirigami.Theme.highlightedTextColor
            }

            Rectangle {
                x: 3.5 * Look.dp
                y: 5.5 * Look.dp
                width: 9 * Look.dp
                height: 2 * Look.dp
                rotation: -45
                antialiasing: true
                color: Kirigami.Theme.highlightedTextColor
            }
        }
    }
}
