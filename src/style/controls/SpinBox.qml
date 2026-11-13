// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import PouStyle

// A number you step: the number in a paper box, with a step on each side.
T.SpinBox {
    id: control

    readonly property color ink: Kirigami.Theme.textColor
    readonly property color ground: Kirigami.Theme.backgroundColor
    readonly property int step: Kirigami.Units.gridUnit + Kirigami.Units.smallSpacing

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + 2 * step + 2 * Kirigami.Units.smallSpacing)
    implicitHeight: Math.max(implicitBackgroundHeight, contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: step + Kirigami.Units.smallSpacing
    rightPadding: step + Kirigami.Units.smallSpacing
    topPadding: Kirigami.Units.smallSpacing
    bottomPadding: Kirigami.Units.smallSpacing
    hoverEnabled: true

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        font: Look.figureFont(control.font, 0.8)
        color: Look.ink
        selectionColor: Kirigami.Theme.highlightColor
        selectedTextColor: control.ground
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        clip: width < implicitWidth
    }

    component Step: Item {
        id: stepItem

        required property bool up
        required property bool pressed
        required property bool hovered

        x: up ? control.width - width : 0
        height: control.height
        implicitWidth: control.step
        implicitHeight: Kirigami.Units.gridUnit

        Text {
            anchors.centerIn: parent
            font: Look.figureFont(control.font, 0.8)
            text: stepItem.up ? "+" : "-"
            color: stepItem.pressed || stepItem.hovered ? Kirigami.Theme.highlightColor
                                                        : Look.tint(control.ground, control.ink, 0.7)
        }
    }

    up.indicator: Step {
        up: true
        pressed: control.up.pressed
        hovered: control.up.hovered
    }

    down.indicator: Step {
        up: false
        pressed: control.down.pressed
        hovered: control.down.hovered
    }

    background: Item {
        implicitWidth: 6 * Kirigami.Units.gridUnit
        implicitHeight: Kirigami.Units.gridUnit + 2 * Kirigami.Units.smallSpacing

        Rectangle {
            x: control.step
            width: parent.width - 2 * control.step
            height: parent.height
            color: Look.paper
            border.width: control.activeFocus ? 2 : 0
            border.color: Kirigami.Theme.highlightColor
        }
    }
}
