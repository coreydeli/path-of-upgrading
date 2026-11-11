// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import PouStyle

// A choice, underlined, with a small teal mark at the end.
T.ComboBox {
    id: control

    readonly property color ink: Kirigami.Theme.textColor
    readonly property color ground: Kirigami.Theme.backgroundColor

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)
    leftPadding: Kirigami.Units.smallSpacing * 2
    rightPadding: (indicator ? indicator.width : 0) + Kirigami.Units.smallSpacing * 2
    topPadding: Kirigami.Units.smallSpacing
    bottomPadding: Kirigami.Units.smallSpacing
    hoverEnabled: true

    delegate: T.ItemDelegate {
        required property var model
        required property int index

        width: ListView.view.width
        implicitHeight: Math.max(implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)
        padding: Kirigami.Units.smallSpacing
        leftPadding: Kirigami.Units.smallSpacing * 2
        hoverEnabled: true
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: model[control.textRole] ?? model.modelData ?? ""
            font: control.font
            color: parent.highlighted ? Kirigami.Theme.backgroundColor : Kirigami.Theme.textColor
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: parent.highlighted ? Kirigami.Theme.highlightColor : "transparent"
        }
    }

    indicator: Item {
        x: control.width - width - Kirigami.Units.smallSpacing
        y: 0
        width: Kirigami.Units.gridUnit
        height: control.height

        // a small solid triangle, drawn so no font has to have it
        Canvas {
            id: mark

            anchors.centerIn: parent
            width: 9
            height: 6
            readonly property color fill: Kirigami.Theme.highlightColor
            onFillChanged: requestPaint()
            onPaint: {
                const g = getContext("2d");
                g.reset();
                g.fillStyle = fill;
                g.beginPath();
                g.moveTo(0, 0);
                g.lineTo(width, 0);
                g.lineTo(width / 2, height);
                g.closePath();
                g.fill();
            }
        }
    }

    contentItem: Text {
        leftPadding: 0
        rightPadding: 0
        text: control.displayText
        font: control.font
        color: control.ink
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.4
    }

    background: Item {
        implicitWidth: 7 * Kirigami.Units.gridUnit
        implicitHeight: Kirigami.Units.gridUnit + 2 * Kirigami.Units.smallSpacing

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: control.hovered || control.visualFocus ? 2 : 1
            color: control.hovered || control.visualFocus ? Kirigami.Theme.highlightColor
                                                          : Look.tint(control.ground, control.ink, 0.55)
        }
    }

    popup: T.Popup {
        y: control.height
        width: control.width
        height: Math.min(contentItem.implicitHeight + 2, control.Window.height - topMargin - bottomMargin)
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
        }

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
            border.width: 1
            border.color: control.ink
        }
    }
}
