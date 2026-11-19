// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A card on a page, drawn by the app: a cut-corner plate with its rows in it,
// or in Material a filled card with rounded corners, and in Cinder
// a square one with a hard shadow.
// The rows go in as they would in a FormCard, and it takes the same
// maximumWidth, which ColumnsPage sets.
Item {
    id: sheet

    default property alias rows: column.data
    property real maximumWidth: Kirigami.Units.gridUnit * 30

    readonly property int pad: 4
    readonly property real side: Kirigami.Units.largeSpacing
    readonly property real panelWidth: Math.min(width - 2 * side, maximumWidth)

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    Layout.fillWidth: true
    implicitWidth: column.implicitWidth + 2 * pad + 2 * side
    implicitHeight: column.implicitHeight + 2 * pad + Kirigami.Units.smallSpacing + Style.shadow

    Item {
        id: panel

        x: Math.round((sheet.width - width) / 2)
        y: Kirigami.Units.smallSpacing
        width: sheet.panelWidth
        height: column.implicitHeight + 2 * sheet.pad

        // a hard shadow, with no blur
        Rectangle {
            visible: Style.shadow > 0
            x: Style.shadow
            y: Style.shadow
            width: parent.width
            height: parent.height
            color: Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.4)
        }

        Plate {
            anchors.fill: parent
            fill: Kirigami.Theme.backgroundColor
            // some looks fill the cards with a tint and leave them unframed
            line: Style.shadow > 0 ? Kirigami.Theme.highlightColor
                                   : Style.framed ? Style.frame(Kirigami.Theme) : "transparent"
            lineWidth: Style.shadow > 0 ? 2 : 1
        }

        ColumnLayout {
            id: column

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: sheet.pad
            }
            spacing: 0
        }
    }
}
