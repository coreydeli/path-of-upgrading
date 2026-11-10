// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

// A double frame to lay over a card, with a bracket and a small diamond at each
// corner. Plain leaves out the corners. It doesn't take the mouse.
Item {
    id: frame

    // how far in the inner line runs
    readonly property int gap: 3
    // the size of a corner's brackets
    readonly property int reach: 10
    // lit, as when the mouse is over what it frames
    property bool lit: false

    enabled: false
    Accessible.ignored: true

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border {
            width: 1
            color: frame.lit ? Style.accent(Kirigami.Theme) : Style.frame(Kirigami.Theme)
        }
    }

    Rectangle {
        anchors {
            fill: parent
            margins: frame.gap
        }
        color: "transparent"
        border {
            width: 1
            color: Style.inner(Kirigami.Theme)
        }
    }

    Repeater {
        model: Style.plain ? 0 : 4

        Item {
            id: corner

            required property int index

            // the corners turn a quarter each, round the frame
            x: index === 0 || index === 3 ? frame.gap : frame.width - frame.gap - width
            y: index < 2 ? frame.gap : frame.height - frame.gap - height
            width: frame.reach
            height: frame.reach
            rotation: corner.index * 90
            transformOrigin: Item.Center

            Rectangle {
                width: corner.width
                height: 1
                color: Style.frame(Kirigami.Theme)
            }

            Rectangle {
                width: 1
                height: corner.height
                color: Style.frame(Kirigami.Theme)
            }

            Rectangle {
                x: -2
                y: -2
                width: 5
                height: 5
                rotation: 45
                color: Style.accent(Kirigami.Theme)
            }
        }
    }
}
