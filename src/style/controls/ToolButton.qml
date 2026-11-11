// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami

// A button without its box until you're on it. Shown by its icon alone, it's
// a drawn cross, as the only one the app has like that closes the popup.
T.ToolButton {
    id: root

    readonly property bool iconOnly: display === T.AbstractButton.IconOnly

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    verticalPadding: Kirigami.Units.smallSpacing
    horizontalPadding: iconOnly ? verticalPadding : Kirigami.Units.largeSpacing
    hoverEnabled: true

    contentItem: Item {
        id: inside

        implicitWidth: root.iconOnly ? Kirigami.Units.gridUnit : words.implicitWidth
        implicitHeight: root.iconOnly ? Kirigami.Units.gridUnit : words.implicitHeight

        ButtonText {
            id: words

            anchors.fill: parent
            visible: !root.iconOnly
            control: root
        }

        Repeater {
            model: root.iconOnly ? 2 : 0

            Rectangle {
                required property int index

                anchors.centerIn: inside
                width: inside.width * 0.8
                height: 1.5
                rotation: index ? -45 : 45
                antialiasing: true
                color: root.hovered ? Kirigami.Theme.highlightColor : Kirigami.Theme.textColor
            }
        }
    }

    background: ButtonFace {
        control: root
        implicitWidth: 0
        visible: !root.iconOnly
    }
}
